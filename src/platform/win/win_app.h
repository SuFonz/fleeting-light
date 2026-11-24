// Win32 宿主：消息循环、窗口、托盘、单实例、桌面挂载、省电检测。
// 它实现 core::IAppHost，把平台事件翻译成 core 能理解的事件。
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/app.h"
#include "platform/win/tray.h"
#include "platform/win/win_util.h"

namespace fl::win {

class WebView2Surface;

class WinApp final : public core::IAppHost {
 public:
  WinApp();
  ~WinApp() override;

  int run(HINSTANCE instance, bool open_settings_on_start);

  // ---- IAppHost ----
  void open_settings() override;
  void close_settings() override;
  bool settings_open() const override { return settings_hwnd_ != nullptr; }
  void set_wallpaper_suspended(bool suspended) override;
  void set_autostart(bool enabled) override;
  bool autostart_enabled() const override;
  void update_tray() override;
  void request_quit() override;
  void open_data_dir() override;
  void open_wallpaper_devtools() override;
  std::string platform_name() const override { return "windows"; }
  std::string webview_version() const override;
  std::string builtin_host() const override { return "fleeting.light"; }
  std::string user_host() const override { return "fleeting.local"; }
  std::string pick_html_file() override;
  std::string pick_folder() override;
  void request_import_wallpaper(bool folder) override;

 private:
  static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT handle_message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  bool create_dispatcher_window();
  bool create_wallpaper_window();
  void destroy_wallpaper_window();
  void start_wallpaper();
  void show_tray_menu(POINT screen_point);
  void handle_tray_command(UINT command);
  void rebuild_tray_icon();
  void poll_fullscreen();
  void update_tooltip();

  HINSTANCE instance_ = nullptr;
  HWND dispatcher_ = nullptr;
  HWND wallpaper_hwnd_ = nullptr;
  HWND settings_hwnd_ = nullptr;

  // 从 exe 资源里加载的应用图标（窗口类 / 任务栏共用）
  HICON icon_large_ = nullptr;
  HICON icon_small_ = nullptr;

  std::shared_ptr<WebView2Surface> wallpaper_;
  std::shared_ptr<WebView2Surface> settings_;

  core::App core_;
  TrayIcon tray_;

  UINT activate_message_ = 0;
  UINT taskbar_created_message_ = 0;
  bool wts_registered_ = false;
  bool quitting_ = false;
  bool last_fullscreen_ = false;
};

}  // namespace fl::win
