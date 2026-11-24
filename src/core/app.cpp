#include "core/app.h"

#include <algorithm>
#include <cmath>

#include "core/log.h"
#include "core/platform_env.h"

namespace fl::core {
namespace {

constexpr const char* kBuiltinManifest = "wallpapers/manifest.json";
constexpr const char* kUserRefPrefix = "user:";
constexpr const char* kIndexFile = "index.html";

// id 只保留 ASCII：它同时是文件夹名和 URL 路径，中文/空格走显示名就行
std::string sanitize_id(std::string_view raw) {
  std::string out;
  bool last_was_dash = false;
  for (char c : raw) {
    char lower = c;
    if (c >= 'A' && c <= 'Z') lower = static_cast<char>(c - 'A' + 'a');
    const bool keep = (lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9') || lower == '_';
    if (keep) {
      out.push_back(lower);
      last_was_dash = false;
    } else if (!last_was_dash && !out.empty()) {
      out.push_back('-');
      last_was_dash = true;
    }
  }
  while (!out.empty() && out.back() == '-') out.pop_back();
  if (out.size() > 48) out.resize(48);
  return out;
}

bool is_user_ref(std::string_view ref) { return ref.rfind(kUserRefPrefix, 0) == 0; }

std::string ref_to_id(std::string_view ref) {
  return is_user_ref(ref) ? std::string(ref.substr(std::string_view(kUserRefPrefix).size())) : std::string(ref);
}

bool valid_user_id(const std::string& id) {
  if (id.empty() || id.size() > 64) return false;
  for (char c : id) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    if (!ok) return false;
  }
  return true;
}

std::string make_state_json(bool paused, const std::string& reason, bool settings_open, bool user_paused) {
  Json s = Json::object();
  s["type"] = "state";
  s["paused"] = paused;
  s["reason"] = reason;
  s["settingsOpen"] = settings_open;
  s["userPaused"] = user_paused;
  return s.dump();
}

}  // namespace

App::App(IAppHost& host) : host_(host), config_(env::join_path(env::user_data_dir(), "config.json")) {}

App::~App() = default;

const std::string& App::config_path() const { return config_.path(); }

std::string App::wallpaper_url() const {
  const std::string ref = config_.wallpaper_id();
  const std::string id = ref_to_id(ref);
  if (is_user_ref(ref)) {
    return "https://" + host_.user_host() + "/" + id + "/" + kIndexFile;
  }
  return "https://" + host_.builtin_host() + "/wallpapers/" + id + "/" + kIndexFile;
}

std::string App::wallpaper_display_name(std::string_view ref) const {
  for (const WallpaperEntry& entry : wallpapers_) {
    if (entry.ref() == ref) return entry.name;
  }
  return std::string(ref);
}

void App::start() {
  std::string error;
  if (!config_.load(&error) && !error.empty()) {
    log_error("加载配置失败: " + error);
  } else {
    log_info("配置已载入: " + config_.path());
  }
  in_user_paused_ = config_.user_paused();
  load_wallpaper_manifest();

  // 配置里指的用户壁纸可能已经被手动删掉了
  const std::string ref = config_.wallpaper_id();
  if (is_user_ref(ref)) {
    const bool found = std::any_of(wallpapers_.begin(), wallpapers_.end(),
                                   [&ref](const WallpaperEntry& entry) { return entry.ref() == ref; });
    if (!found) {
      log_warn("壁纸 " + ref + " 已不存在，回退到内置壁纸");
      config_.set_wallpaper_id("aurora");
    }
  }

  started_ = true;
  recompute_state();
  log_info("核心已启动，壁纸: " + config_.wallpaper_id());
}

void App::load_wallpaper_manifest() {
  wallpapers_.clear();

  // 1) 内置壁纸：仓库里的 web/wallpapers/manifest.json
  const std::string path = env::join_path(env::web_root(), kBuiltinManifest);
  std::string text;
  std::string error;
  if (!env::read_file(path, &text, &error)) {
    log_warn("读取内置壁纸清单失败: " + path + " (" + error + ")");
  } else {
    std::string parse_error;
    const Json root = Json::parse(text, &parse_error);
    if (!parse_error.empty()) {
      log_warn("内置壁纸清单解析失败: " + parse_error);
    } else {
      for (const Json& item : root.elements()) {
        WallpaperEntry entry;
        entry.id = item.at("id").as_string();
        if (entry.id.empty()) continue;
        entry.name = item.at("name").as_string(entry.id);
        entry.description = item.at("description").as_string();
        entry.user = false;
        wallpapers_.push_back(std::move(entry));
      }
    }
  }
  if (wallpapers_.empty()) {
    wallpapers_.push_back(WallpaperEntry{"aurora", "极光", "内置", false});
  }

  // 2) 用户导入的壁纸：扫描 <data>/wallpapers 下的子目录
  std::vector<std::string> dirs;
  env::list_dirs(env::user_wallpapers_dir(), &dirs, nullptr);
  std::sort(dirs.begin(), dirs.end());
  for (const std::string& name : dirs) {
    if (!valid_user_id(name)) continue;
    const std::string dir = env::join_path(env::user_wallpapers_dir(), name);
    if (!env::file_exists(env::join_path(dir, kIndexFile))) continue;

    WallpaperEntry entry;
    entry.id = name;
    entry.user = true;
    entry.name = name;
    entry.description = "导入的壁纸";

    std::string meta_text;
    if (env::read_file(env::join_path(dir, "wallpaper.json"), &meta_text, nullptr)) {
      std::string parse_error;
      const Json meta = Json::parse(meta_text, &parse_error);
      if (parse_error.empty()) {
        const std::string meta_name = meta.at("name").as_string();
        if (!meta_name.empty()) entry.name = meta_name;
        const std::string meta_description = meta.at("description").as_string();
        if (!meta_description.empty()) entry.description = meta_description;
      }
    }
    wallpapers_.push_back(std::move(entry));
  }
}

void App::attach_wallpaper(IViewSurface* surface) {
  wallpaper_ = surface;
  if (!surface) return;
  surface->create(wallpaper_url(), [this](std::string_view json) { handle_wallpaper_message(json); },
                  [this] {
                    log_info("壁纸视图已就绪");
                    push_wallpaper_init();
                  });
  apply_suspension();
  push_wallpaper_init();
}

void App::detach_wallpaper() { wallpaper_ = nullptr; }

void App::attach_settings(IViewSurface* surface) {
  settings_ = surface;
  if (!surface) return;
  surface->create("settings/index.html", [this](std::string_view json) { handle_settings_message(json); },
                  [this] {
                    log_info("设置视图已就绪");
                    push_settings_init();
                  });
  push_settings_init();
}

void App::detach_settings() { settings_ = nullptr; }

// ---------------------------------------------------------------- 状态机

void App::recompute_state() {
  std::string reason;
  if (in_user_paused_) {
    reason = "user";
  } else if (in_suspend_) {
    reason = "suspend";
  } else if (in_locked_ && config_.pause_on_lock()) {
    reason = "lock";
  } else if (in_fullscreen_ && config_.pause_on_fullscreen()) {
    reason = "fullscreen";
  } else if (in_settings_open_ && config_.pause_when_settings_open()) {
    reason = "settings";
  }

  const bool paused = !reason.empty();
  const bool changed = (paused != state_paused_) || (reason != pause_reason_);
  state_paused_ = paused;
  pause_reason_ = reason;

  // 只有“看不见”的暂停才真正隐藏视图；用户手动暂停要留着画面
  const bool should_suspend = paused && reason != "user";
  if (should_suspend != suspended_) {
    suspended_ = should_suspend;
    apply_suspension();
  }

  if (changed) {
    log_info(std::string("暂停状态: ") + (paused ? ("开 (" + reason + ")") : "关"));
    host_.update_tray();
  }
  broadcast_state();
}

void App::apply_suspension() {
  if (wallpaper_) wallpaper_->set_visible(!suspended_);
  if (!started_) return;
  host_.set_wallpaper_suspended(suspended_);
}void App::broadcast_state() {
  const std::string state = make_state_json(state_paused_, pause_reason_, in_settings_open_, in_user_paused_);
  if (wallpaper_) wallpaper_->send(state);
  if (settings_) settings_->send(state);
}

void App::broadcast_config() {
  const std::string payload = "{\"type\":\"config\",\"config\":" + config_.data().dump() + "}";
  if (wallpaper_) wallpaper_->send(payload);
  if (settings_) settings_->send(payload);
}

void App::push_wallpaper_init() {
  if (!wallpaper_) return;
  Json msg = Json::object();
  msg["type"] = "init";
  msg["config"] = config_.data();
  msg["state"]["paused"] = state_paused_;
  msg["state"]["reason"] = pause_reason_;
  msg["state"]["settingsOpen"] = in_settings_open_;
  msg["state"]["userPaused"] = in_user_paused_;
  msg["wallpaperId"] = config_.wallpaper_id();
  msg["fps"] = config_.wallpaper_fps();
  msg["version"] = env::app_version();
  wallpaper_->send(msg.dump());
}

void App::push_settings_init() {
  if (!settings_) return;
  Json msg = Json::object();
  msg["type"] = "init";
  msg["config"] = config_.data();

  Json state = Json::object();
  state["paused"] = state_paused_;
  state["reason"] = pause_reason_;
  state["settingsOpen"] = in_settings_open_;
  state["userPaused"] = in_user_paused_;
  msg["state"] = state;

  Json info = Json::object();
  info["version"] = env::app_version();
  info["platform"] = host_.platform_name();
  info["webview"] = host_.webview_version();
  info["configPath"] = config_.path();
  info["dataDir"] = env::user_data_dir();
  info["webRoot"] = env::web_root();
  info["userWallpapersDir"] = env::user_wallpapers_dir();
  info["autostart"] = host_.autostart_enabled();

  Json list = Json::array();
  for (const WallpaperEntry& entry : wallpapers_) {
    Json item = Json::object();
    item["id"] = entry.ref();
    item["name"] = entry.name;
    item["description"] = entry.description;
    item["user"] = entry.user;
    item["active"] = (entry.ref() == config_.wallpaper_id());
    list.push(std::move(item));
  }
  info["wallpapers"] = list;
  msg["info"] = info;

  settings_->send(msg.dump());
}

void App::push_settings_toast(const char* level, std::string message) {
  if (!settings_) return;
  Json msg = Json::object();
  msg["type"] = "toast";
  msg["level"] = level;
  msg["message"] = std::move(message);
  settings_->send(msg.dump());
}

// ---------------------------------------------------------------- 平台事件

void App::on_fullscreen_changed(bool fullscreen) {
  if (in_fullscreen_ == fullscreen) return;
  in_fullscreen_ = fullscreen;
  recompute_state();
}

void App::on_locked(bool locked) {
  if (in_locked_ == locked) return;
  in_locked_ = locked;
  recompute_state();
}

void App::on_system_suspend(bool suspended) {
  if (in_suspend_ == suspended) return;
  in_suspend_ = suspended;
  recompute_state();
}

void App::on_settings_opened() {
  in_settings_open_ = true;
  recompute_state();
  if (settings_) push_settings_init();
}

void App::on_settings_closed() {
  in_settings_open_ = false;
  recompute_state();
}

void App::toggle_user_pause() { set_user_paused(!in_user_paused_); }

void App::set_user_paused(bool paused) {
  if (in_user_paused_ == paused) return;
  in_user_paused_ = paused;
  config_.set_user_paused(paused);
  recompute_state();
  push_settings_toast("info", paused ? "壁纸已暂停" : "壁纸已继续");
}

void App::reload_wallpaper() {
  if (!wallpaper_) return;
  wallpaper_->navigate(wallpaper_url());
  push_wallpaper_init();
  log_info("已重新加载壁纸");
}

// ---------------------------------------------------------------- 桌面互动

bool App::wallpaper_interaction_enabled() const {
  return started_ && wallpaper_ != nullptr && !state_paused_ && config_.pointer_tracking();
}

void App::on_pointer(float x, float y, bool inside, bool down) {
  if (!wallpaper_interaction_enabled()) return;

  // 光标没怎么动、按键状态也没变，就不发消息，省得每秒几十条无意义消息
  const bool moved = std::fabs(x - pointer_x_) > 0.0015f || std::fabs(y - pointer_y_) > 0.0015f;
  if (!moved && down == pointer_down_ && inside == pointer_inside_) return;

  pointer_x_ = x;
  pointer_y_ = y;
  pointer_inside_ = inside;
  pointer_down_ = down;

  Json msg = Json::object();
  msg["type"] = "pointer";
  msg["x"] = static_cast<double>(x);
  msg["y"] = static_cast<double>(y);
  msg["inside"] = inside;
  msg["down"] = down;
  wallpaper_->send(msg.dump());
}

void App::switch_wallpaper(std::string ref) {
  if (ref == config_.wallpaper_id()) return;
  config_.set_wallpaper_id(ref);
  if (wallpaper_) {
    wallpaper_->navigate(wallpaper_url());
    push_wallpaper_init();
  }
  log_info("切换壁纸: " + ref);
  broadcast_config();
}

void App::reset_config() {
  std::string error;
  if (!config_.reset_to_defaults(&error)) {
    push_settings_toast("error", "重置失败: " + error);
    return;
  }
  in_user_paused_ = config_.user_paused();
  if (wallpaper_) {
    wallpaper_->navigate(wallpaper_url());
    push_wallpaper_init();
  }
  broadcast_config();
  recompute_state();
  push_settings_init();
  push_settings_toast("success", "已恢复默认设置");
}

// ---------------------------------------------------------------- 壁纸导入

void App::import_wallpaper(bool folder) {
  const std::string picked = folder ? host_.pick_folder() : host_.pick_html_file();
  if (picked.empty()) return;  // 用户取消

  const std::string base = folder ? env::base_name(picked) : env::file_stem(picked);
  std::string slug = sanitize_id(base);
  if (slug.empty()) slug = "wallpaper";

  std::string id = slug;
  for (int index = 2; index < 1000; ++index) {
    if (!env::dir_exists(env::join_path(env::user_wallpapers_dir(), id))) break;
    id = slug + "-" + std::to_string(index);
  }

  const std::string dir = env::join_path(env::user_wallpapers_dir(), id);
  if (env::dir_exists(dir)) {
    push_settings_toast("error", "同名壁纸已存在，请先移除或改名");
    return;
  }

  std::string error;
  if (folder) {
    const bool has_index = env::file_exists(env::join_path(picked, kIndexFile));
    const bool has_index_htm = env::file_exists(env::join_path(picked, "index.htm"));
    if (!has_index && !has_index_htm) {
      push_settings_toast("error", "这个文件夹里没有 index.html，请选网页构建产物的根目录");
      return;
    }
    if (!env::copy_tree(picked, dir, &error)) {
      push_settings_toast("error", "复制文件夹失败：" + error);
      env::remove_tree(dir, nullptr);
      return;
    }
    if (!env::file_exists(env::join_path(dir, kIndexFile))) {
      env::copy_file(env::join_path(dir, "index.htm"), env::join_path(dir, kIndexFile), nullptr);
    }
  } else {
    if (!env::ensure_dir(dir)) {
      push_settings_toast("error", "无法创建壁纸目录：" + env::user_wallpapers_dir());
      return;
    }
    if (!env::copy_file(picked, env::join_path(dir, kIndexFile), &error)) {
      push_settings_toast("error", "复制文件失败：" + error);
      env::remove_tree(dir, nullptr);
      return;
    }
  }

  const std::string display_name = base.empty() ? id : base;
  Json meta = Json::object();
  meta["name"] = display_name;
  meta["description"] = folder ? "导入的网页文件夹" : "导入的 HTML 文件";
  env::write_file(env::join_path(dir, "wallpaper.json"), meta.dump(2), env::FileMode::Write, nullptr);

  log_info("导入壁纸: " + id + " <- " + picked);
  load_wallpaper_manifest();
  switch_wallpaper(std::string(kUserRefPrefix) + id);
  push_settings_init();
  push_settings_toast("success", "已添加壁纸：" + display_name);
}

void App::remove_wallpaper(std::string_view ref) {
  if (!is_user_ref(ref)) {
    push_settings_toast("error", "内置壁纸不能移除");
    return;
  }
  const std::string id = ref_to_id(ref);
  if (!valid_user_id(id)) {
    push_settings_toast("error", "壁纸 id 非法");
    return;
  }

  const std::string dir = env::join_path(env::user_wallpapers_dir(), id);
  std::string error;
  if (!env::remove_tree(dir, &error)) {
    push_settings_toast("error", "移除失败：" + error);
    return;
  }

  const bool was_active = (config_.wallpaper_id() == std::string(ref));
  load_wallpaper_manifest();
  if (was_active) switch_wallpaper("aurora");
  push_settings_init();
  push_settings_toast("success", "已移除壁纸");
  log_info("移除壁纸: " + std::string(ref));
}

// ---------------------------------------------------------------- 消息处理

void App::handle_wallpaper_message(std::string_view json) {
  std::string parse_error;
  const Json msg = Json::parse(json, &parse_error);
  if (!parse_error.empty()) {
    log_warn(std::string("壁纸消息解析失败: ") + parse_error);
    return;
  }
  const std::string type = msg.at("type").as_string();
  if (type == "ready") {
    log_info("壁纸页面 ready");
    push_wallpaper_init();
    recompute_state();
  } else if (type == "request_init") {
    push_wallpaper_init();
  } else if (type == "log") {
    const std::string level = msg.at("level").as_string("info");
    const std::string text = msg.at("message").as_string();
    if (level == "error") log_error("[壁纸] " + text);
    else if (level == "warn") log_warn("[壁纸] " + text);
    else log_info("[壁纸] " + text);
  } else if (type == "stats") {
    const double fps = msg.at("fps").as_double(0.0);
    const double draw_ms = msg.at("drawMs").as_double(0.0);
    log_debug("壁纸帧率: " + std::to_string(static_cast<int>(fps + 0.5)));
    // 顺手转给设置面板，让它能显示实时帧率
    if (settings_) {
      Json relay = Json::object();
      relay["type"] = "stats";
      relay["fps"] = fps;
      relay["drawMs"] = draw_ms;
      settings_->send(relay.dump());
    }
  }
}

void App::handle_settings_message(std::string_view json) {
  std::string parse_error;
  const Json msg = Json::parse(json, &parse_error);
  if (!parse_error.empty()) {
    log_warn(std::string("设置消息解析失败: ") + parse_error);
    return;
  }
  const std::string type = msg.at("type").as_string();

  if (type == "ready") {
    push_settings_init();
    recompute_state();
    return;
  }
  if (type == "request_init") {
    push_settings_init();
    return;
  }
  if (type == "update") {
    const Json& patch = msg.at("config");
    if (!patch.is_object()) {
      push_settings_toast("error", "配置补丁格式错误");
      return;
    }
    const std::string previous_id = config_.wallpaper_id();
    const bool previous_autostart = config_.start_with_windows();
    std::string error;
    if (!config_.apply_patch(patch, &error)) {
      log_warn("保存配置失败: " + error);
      push_settings_toast("error", "保存配置失败: " + error);
      return;
    }
    if (config_.start_with_windows() != previous_autostart) {
      host_.set_autostart(config_.start_with_windows());
    }
    if (config_.wallpaper_id() != previous_id) {
      load_wallpaper_manifest();
      if (wallpaper_) {
        wallpaper_->navigate(wallpaper_url());
        push_wallpaper_init();
      }
      log_info("切换壁纸: " + config_.wallpaper_id());
    }
    broadcast_config();
    recompute_state();
    push_settings_toast("success", "设置已保存");
    return;
  }
  if (type == "set_paused") {
    set_user_paused(msg.at("paused").as_bool(!in_user_paused_));
    return;
  }
  if (type == "action") {
    handle_action(msg.at("name").as_string(), msg);
    return;
  }
  log_debug("设置面板未知消息: " + type);
}

void App::handle_action(std::string_view name, const Json& payload) {
  if (name == "quit") {
    host_.request_quit();
  } else if (name == "restart_wallpaper") {
    reload_wallpaper();
    push_settings_toast("success", "壁纸已重启");
  } else if (name == "reset_config") {
    reset_config();
  } else if (name == "open_data_dir") {
    host_.open_data_dir();
  } else if (name == "open_wallpaper_devtools") {
    host_.open_wallpaper_devtools();
  } else if (name == "close") {
    host_.close_settings();
  } else if (name == "import_wallpaper") {
    host_.request_import_wallpaper(payload.at("kind").as_string("folder") == "folder");
  } else if (name == "remove_wallpaper") {
    remove_wallpaper(payload.at("id").as_string());
  } else if (name == "set_autostart") {
    const bool enabled = payload.at("enabled").as_bool(false);
    host_.set_autostart(enabled);
    Json patch = Json::object();
    patch["app"]["startWithWindows"] = enabled;
    config_.apply_patch(patch, nullptr);
    broadcast_config();
    push_settings_toast("success", enabled ? "已设置开机自启" : "已取消开机自启");
  } else if (name == "refresh_manifest") {
    load_wallpaper_manifest();
    push_settings_init();
  }
}

}  // namespace fl::core
