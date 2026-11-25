#include "core/config.h"

#include <algorithm>

#include "core/log.h"
#include "core/platform_env.h"

namespace fl::core {

const Json& ConfigCenter::defaults() {
  static const Json kDefaults = [] {
    Json j = Json::object();
    j["version"] = 1;
    j["wallpaper"]["id"] = "matrix";
    j["wallpaper"]["enabled"] = true;
    j["wallpaper"]["fps"] = 60;
    j["wallpaper"]["params"] = Json::object();
    j["app"]["startWithWindows"] = false;
    j["app"]["pauseOnFullscreen"] = true;
    j["app"]["pauseOnLock"] = true;
    j["app"]["pauseWhenSettingsOpen"] = false;
    j["app"]["devTools"] = false;
    j["app"]["pointerTracking"] = true;
    return j;
  }();
  return kDefaults;
}

Json ConfigCenter::normalize(const Json& input) {
  Json out = defaults();
  out.merge_patch(input);

  // wallpaper.id：非空字符串
  std::string id = out.get_path("wallpaper.id").as_string();
  if (id.empty()) id = "matrix";
  out["wallpaper"]["id"] = id;

  // fps：0 表示跟随显示器，1..240 为有效帧率上限
  long long fps = out.get_path("wallpaper.fps").as_int(60);
  fps = std::clamp<long long>(fps, 0, 240);
  out["wallpaper"]["fps"] = static_cast<int>(fps);

  // 兼容旧配置：以前是 "paused": true，语义正好反过来
  {
    Json& wp = out["wallpaper"];
    if (wp.contains("paused")) {
      if (!wp.contains("enabled")) wp["enabled"] = !wp["paused"].as_bool(false);
      wp.erase("paused");
    }
  }

  // 布尔项强制类型（用户手改配置写错类型时回退到默认值）
  const char* bool_paths[] = {"wallpaper.enabled",  "app.startWithWindows",      "app.pauseOnFullscreen",
                              "app.pauseOnLock",    "app.pauseWhenSettingsOpen", "app.devTools",
                              "app.pointerTracking"};
  for (const char* path : bool_paths) {
    const std::string_view p{path};
    const std::size_t dot = p.find('.');
    const std::string_view group = p.substr(0, dot);
    const std::string_view key = p.substr(dot + 1);
    const bool value = out.get_path(p).as_bool(defaults().get_path(p).as_bool());
    out[group][key] = value;
  }

  if (!out.get_path("wallpaper.params").is_object()) {
    out["wallpaper"]["params"] = Json::object();
  }
  out["version"] = 1;
  return out;
}

ConfigCenter::ConfigCenter(std::string path) : path_(std::move(path)), data_(defaults()) {}

bool ConfigCenter::load(std::string* error) {
  std::string text;
  if (!env::read_file(path_, &text, error)) {
    if (!env::file_exists(path_)) {
      data_ = normalize(Json());
      std::string save_error;
      if (!save(&save_error)) log_warn("写入默认配置失败: " + save_error);
      return true;  // 首次运行，用默认值
    }
    return false;
  }
  std::string parse_error;
  Json parsed = Json::parse(text, &parse_error);
  if (!parse_error.empty()) {
    if (error) *error = "config.json 解析失败: " + parse_error;
    data_ = normalize(Json());
    return false;
  }
  data_ = normalize(parsed);
  return true;
}

bool ConfigCenter::save(std::string* error) const {
  env::ensure_dir(env::user_data_dir());
  return env::write_file(path_, data_.dump(2) + "\n", env::FileMode::Write, error);
}

bool ConfigCenter::apply_patch(const Json& patch, std::string* error) {
  Json next = data_;
  next.merge_patch(patch);
  data_ = normalize(next);
  return save(error);
}

bool ConfigCenter::reset_to_defaults(std::string* error) {
  data_ = normalize(Json());
  return save(error);
}

std::string ConfigCenter::wallpaper_id() const { return data_.get_path("wallpaper.id").as_string("matrix"); }

int ConfigCenter::wallpaper_fps() const { return static_cast<int>(data_.get_path("wallpaper.fps").as_int(60)); }

bool ConfigCenter::wallpaper_enabled() const { return data_.get_path("wallpaper.enabled").as_bool(true); }

bool ConfigCenter::pause_on_fullscreen() const { return data_.get_path("app.pauseOnFullscreen").as_bool(true); }

bool ConfigCenter::pause_on_lock() const { return data_.get_path("app.pauseOnLock").as_bool(true); }

bool ConfigCenter::pause_when_settings_open() const {
  return data_.get_path("app.pauseWhenSettingsOpen").as_bool(false);
}

bool ConfigCenter::dev_tools() const { return data_.get_path("app.devTools").as_bool(false); }

bool ConfigCenter::start_with_windows() const { return data_.get_path("app.startWithWindows").as_bool(false); }

bool ConfigCenter::pointer_tracking() const { return data_.get_path("app.pointerTracking").as_bool(true); }

void ConfigCenter::set_wallpaper_enabled(bool enabled) {
  data_["wallpaper"]["enabled"] = enabled;
  std::string error;
  if (!save(&error)) log_warn("保存壁纸启用状态失败: " + error);
}

void ConfigCenter::set_wallpaper_id(std::string id) {
  data_["wallpaper"]["id"] = std::move(id);
  std::string error;
  if (!save(&error)) log_warn("保存壁纸 id 失败: " + error);
}

}  // namespace fl::core
