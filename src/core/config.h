// 全局配置中心：config.json 的读写 + 内存中的最新状态。
// 配置结构（见 defaults()）：
// {
//   "version": 1,
//   "wallpaper": { "id": "matrix", "paused": false, "fps": 60, "params": {} },
//   "app": { "startWithWindows": false, "pauseOnFullscreen": true, "pauseOnLock": true,
//            "pauseWhenSettingsOpen": false, "devTools": false }
// }
#pragma once

#include <string>

#include "core/json.h"

namespace fl::core {

class ConfigCenter {
 public:
  explicit ConfigCenter(std::string path);

  // 读取配置；文件缺失时写入一份默认配置
  bool load(std::string* error);
  bool save(std::string* error) const;

  const Json& data() const { return data_; }
  static const Json& defaults();
  const std::string& path() const { return path_; }

  // 深合并一个补丁并落盘（null 表示删除该键）
  bool apply_patch(const Json& patch, std::string* error);
  bool reset_to_defaults(std::string* error);

  // ---- 常用字段 ----
  std::string wallpaper_id() const;
  int wallpaper_fps() const;
  bool wallpaper_enabled() const;
  bool pause_on_fullscreen() const;
  bool pause_on_lock() const;
  bool pause_when_settings_open() const;
  bool dev_tools() const;
  bool start_with_windows() const;
  bool pointer_tracking() const;

  void set_wallpaper_enabled(bool enabled);
  void set_wallpaper_id(std::string id);

 private:
  // 归一化：补全缺省项、裁剪非法值
  static Json normalize(const Json& input);

  std::string path_;
  Json data_;
};

}  // namespace fl::core
