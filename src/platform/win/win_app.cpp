#include "platform/win/win_app.h"

#include <shellapi.h>
#include <shlobj.h>
#include <wtsapi32.h>

#include <algorithm>
#include <string>

#include "core/config.h"
#include "core/log.h"
#include "core/platform_env.h"
#include "platform/win/app_resources.h"
#include "platform/win/autostart.h"
#include "platform/win/desktop_host.h"
#include "platform/win/wallpaper_picker.h"
#include "platform/win/webview2_env.h"
#include "platform/win/webview2_surface.h"

namespace fl::win {
namespace {

constexpr wchar_t kDispatcherClass[] = L"FleetingLight.Dispatcher";
constexpr wchar_t kWallpaperClass[] = L"FleetingLight.Wallpaper";
constexpr wchar_t kSettingsClass[] = L"FleetingLight.Settings";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kImportWallpaperMessage = WM_APP + 2;
constexpr UINT_PTR kTimerFullscreen = 1;
constexpr UINT_PTR kTimerWatchdog = 2;
constexpr int kHotkeyToggleSettings = 1;

enum MenuCommand : UINT {
  kMenuSettings = 1,
  kMenuPause,
  kMenuReload,
  kMenuAutostart,
  kMenuDevTools,
  kMenuDataDir,
  kMenuQuit,
};

bool query_fullscreen_state() {
  QUERY_USER_NOTIFICATION_STATE state = QUNS_ACCEPTS_NOTIFICATIONS;
  if (FAILED(SHQueryUserNotificationState(&state))) return false;
  switch (state) {
    case QUNS_BUSY:
    case QUNS_RUNNING_D3D_FULL_SCREEN:
    case QUNS_PRESENTATION_MODE:
      return true;
    default:
      return false;
  }
}

std::wstring tooltip_for(const core::App& core) {
  std::wstring text = L"Fleeting Light";
  if (!core.paused()) return text + L" · 运行中";
  const std::string& reason = core.pause_reason();
  if (reason == "user") return text + L" · 已暂停";
  if (reason == "fullscreen") return text + L" · 全屏遮挡，暂停中";
  if (reason == "lock") return text + L" · 锁屏，暂停中";
  if (reason == "suspend") return text + L" · 休眠，暂停中";
  return text + L" · 暂停中";
}

}  // namespace

WinApp::WinApp() : core_(*this) {}

WinApp::~WinApp() = default;

// ---------------------------------------------------------------- 启动/主循环

int WinApp::run(HINSTANCE instance, bool open_settings_on_start) {
  instance_ = instance;
  activate_message_ = RegisterWindowMessageW(L"FleetingLight.Activate");
  taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");

  // 应用图标：大图标给任务栏/Alt-Tab，小图标给标题栏
  icon_large_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(FL_IDI_APP), IMAGE_ICON,
                                              GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
                                              LR_DEFAULTCOLOR));
  icon_small_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(FL_IDI_APP), IMAGE_ICON,
                                              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                              LR_DEFAULTCOLOR));
  if (!icon_large_ && !icon_small_) {
    log_warn("没能从资源里加载应用图标（windres 未参与构建？）");
  }

  auto register_class = [this](const wchar_t* name, HBRUSH brush, HCURSOR cursor) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &WinApp::window_proc;
    wc.hInstance = instance_;
    wc.hCursor = cursor ? cursor : LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = brush;
    wc.hIcon = icon_large_;
    wc.hIconSm = icon_small_ ? icon_small_ : icon_large_;
    wc.lpszClassName = name;
    return RegisterClassExW(&wc) != 0;
  };

  register_class(kDispatcherClass, nullptr, nullptr);
  register_class(kWallpaperClass, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr);
  register_class(kSettingsClass, CreateSolidBrush(RGB(11, 13, 20)), LoadCursorW(nullptr, IDC_ARROW));

  if (!create_dispatcher_window()) {
    MessageBoxW(nullptr, L"无法创建主窗口", L"Fleeting Light", MB_ICONERROR);
    return 1;
  }

  if (WTSRegisterSessionNotification(dispatcher_, NOTIFY_FOR_THIS_SESSION)) {
    wts_registered_ = true;
  } else {
    log_warn("WTSRegisterSessionNotification 失败，锁屏暂停不可用");
  }

  std::string error;
  if (!WebView2Env::instance().initialize(&error)) {
    const std::wstring message = to_wide(
        error + "\n\n请安装 Microsoft Edge WebView2 运行时后重试：\n"
                "https://developer.microsoft.com/microsoft-edge/webview2/");
    MessageBoxW(nullptr, message.c_str(), L"Fleeting Light", MB_ICONERROR);
    return 1;
  }

  std::wstring arguments =
      L"--autoplay-policy=no-user-gesture-required "
      L"--disable-background-timer-throttling "
      L"--disable-backgrounding-occluded-windows "
      L"--disable-renderer-backgrounding "
      L"--disable-features=CalculateNativeWinOcclusion";
  {
    wchar_t dev[8] = {};
    if (GetEnvironmentVariableW(L"FL_DEV", dev, static_cast<DWORD>(std::size(dev))) > 0) {
      arguments += L" --remote-debugging-port=9222";
    }
  }
  WebView2Env::instance().set_browser_arguments(arguments);
  WebView2Env::instance().set_user_data_folder(to_wide(env::join_path(env::user_data_dir(), "WebView2")));

  rebuild_tray_icon();
  SetTimer(dispatcher_, kTimerFullscreen, 1500, nullptr);
  SetTimer(dispatcher_, kTimerWatchdog, 4000, nullptr);
  RegisterHotKey(dispatcher_, kHotkeyToggleSettings, MOD_CONTROL | MOD_ALT, VK_F9);

  core_.start();
  start_wallpaper();
  if (open_settings_on_start) open_settings();
  update_tooltip();

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  quitting_ = true;
  KillTimer(dispatcher_, kTimerFullscreen);
  KillTimer(dispatcher_, kTimerWatchdog);
  UnregisterHotKey(dispatcher_, kHotkeyToggleSettings);
  if (wts_registered_) {
    WTSUnRegisterSessionNotification(dispatcher_);
    wts_registered_ = false;
  }
  tray_.remove();
  destroy_wallpaper_window();
  if (settings_hwnd_) {
    HWND hwnd = settings_hwnd_;
    settings_hwnd_ = nullptr;
    DestroyWindow(hwnd);
  }
  if (settings_) {
    settings_->destroy();
    settings_.reset();
  }
  if (dispatcher_) {
    DestroyWindow(dispatcher_);
    dispatcher_ = nullptr;
  }
  if (icon_small_) {
    DestroyIcon(icon_small_);
    icon_small_ = nullptr;
  }
  if (icon_large_) {
    DestroyIcon(icon_large_);
    icon_large_ = nullptr;
  }
  log_info("退出");
  return 0;
}

bool WinApp::create_dispatcher_window() {
  dispatcher_ = CreateWindowExW(0, kDispatcherClass, L"Fleeting Light Dispatcher", WS_POPUP, 0, 0, 0, 0, nullptr,
                                nullptr, instance_, this);
  return dispatcher_ != nullptr;
}

// ---------------------------------------------------------------- 窗口过程

LRESULT CALLBACK WinApp::window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  WinApp* self = reinterpret_cast<WinApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<WinApp*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  if (!self) return DefWindowProcW(hwnd, message, wparam, lparam);
  return self->handle_message(hwnd, message, wparam, lparam);
}

LRESULT WinApp::handle_message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  // ---- 隐藏的分发窗口 ----
  if (hwnd == dispatcher_) {
    if (message == kTrayMessage) {
      switch (LOWORD(lparam)) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
          open_settings();
          return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: {
          POINT point{};
          GetCursorPos(&point);
          show_tray_menu(point);
          return 0;
        }
        default: return 0;
      }
    }
    if (message == WM_TIMER) {
      if (wparam == kTimerFullscreen) {
        poll_fullscreen();
      } else if (wparam == kTimerWatchdog && !quitting_) {
        if (!wallpaper_hwnd_ || !IsWindow(wallpaper_hwnd_)) {
          log_warn("壁纸窗口已失效（Explorer 可能重启），正在重建");
          start_wallpaper();
        }
      }
      return 0;
    }
    if (activate_message_ != 0 && message == activate_message_) {
      open_settings();
      return 0;
    }
    if (message == kImportWallpaperMessage) {
      // 从消息队列里弹选择器：不在 WebView2 的回调栈上开模态对话框
      core_.import_wallpaper(wparam != 0);
      return 0;
    }
    if (taskbar_created_message_ != 0 && message == taskbar_created_message_) {
      log_info("检测到任务栏重建（Explorer 重启）");
      rebuild_tray_icon();
      if (!wallpaper_hwnd_ || !IsWindow(wallpaper_hwnd_)) start_wallpaper();
      return 0;
    }
    if (message == WM_HOTKEY && wparam == kHotkeyToggleSettings) {
      if (settings_open()) {
        close_settings();
      } else {
        open_settings();
      }
      return 0;
    }
    if (message == WM_WTSSESSION_CHANGE) {
      if (wparam == WTS_SESSION_LOCK) {
        core_.on_locked(true);
      } else if (wparam == WTS_SESSION_UNLOCK) {
        core_.on_locked(false);
      }
      return 0;
    }
    if (message == WM_POWERBROADCAST) {
      if (wparam == PBT_APMSUSPEND) {
        core_.on_system_suspend(true);
      } else if (wparam == PBT_APMRESUMEAUTOMATIC || wparam == PBT_APMRESUMESUSPEND) {
        core_.on_system_suspend(false);
      }
      return TRUE;
    }
    if (message == WM_DESTROY) {
      PostQuitMessage(0);
      return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }

  // ---- 壁纸窗口 ----
  if (hwnd == wallpaper_hwnd_) {
    switch (message) {
      case WM_SIZE:
        if (wallpaper_) wallpaper_->resize(LOWORD(lparam), HIWORD(lparam));
        return 0;
      case WM_DESTROY:
        wallpaper_hwnd_ = nullptr;
        if (wallpaper_) {
          wallpaper_->destroy();
          wallpaper_.reset();
        }
        core_.detach_wallpaper();
        return 0;
      case WM_ERASEBKGND: return 1;
      default: return DefWindowProcW(hwnd, message, wparam, lparam);
    }
  }

  // ---- 设置窗口 ----
  if (hwnd == settings_hwnd_) {
    switch (message) {
      case WM_SIZE:
        if (settings_) settings_->resize(LOWORD(lparam), HIWORD(lparam));
        return 0;
      case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
      case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = 880;
        info->ptMinTrackSize.y = 600;
        return 0;
      }
      case WM_ERASEBKGND: return 1;
      case WM_DESTROY:
        settings_hwnd_ = nullptr;
        if (settings_) {
          settings_->destroy();
          settings_.reset();
        }
        core_.detach_settings();
        core_.on_settings_closed();
        return 0;
      default: return DefWindowProcW(hwnd, message, wparam, lparam);
    }
  }

  return DefWindowProcW(hwnd, message, wparam, lparam);
}

// ---------------------------------------------------------------- 托盘

void WinApp::rebuild_tray_icon() {
  if (!dispatcher_) return;
  tray_.remove();
  if (!tray_.add(dispatcher_, kTrayMessage, tooltip_for(core_))) {
    log_warn("托盘图标创建失败");
  }
}

void WinApp::update_tooltip() { tray_.update_tooltip(tooltip_for(core_)); }

void WinApp::update_tray() { update_tooltip(); }

void WinApp::show_tray_menu(POINT screen_point) {
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, kMenuSettings, L"打开设置面板\tCtrl+Alt+F9");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (core_.paused() ? MF_CHECKED : MF_UNCHECKED), kMenuPause, L"暂停壁纸");
  AppendMenuW(menu, MF_STRING, kMenuReload, L"重新加载壁纸");
  AppendMenuW(menu, MF_STRING, kMenuDevTools, L"壁纸开发者工具");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (autostart_enabled() ? MF_CHECKED : MF_UNCHECKED), kMenuAutostart, L"开机自启");
  AppendMenuW(menu, MF_STRING, kMenuDataDir, L"打开配置目录");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuQuit, L"退出");

  SetForegroundWindow(dispatcher_);
  const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen_point.x, screen_point.y, 0,
                                      dispatcher_, nullptr);
  DestroyMenu(menu);
  PostMessageW(dispatcher_, WM_NULL, 0, 0);
  if (command) handle_tray_command(command);
}

void WinApp::handle_tray_command(UINT command) {
  switch (command) {
    case kMenuSettings: open_settings(); break;
    case kMenuPause: core_.toggle_user_pause(); break;
    case kMenuReload: core_.reload_wallpaper(); break;
    case kMenuDevTools: open_wallpaper_devtools(); break;
    case kMenuAutostart: set_autostart(!autostart_enabled()); break;
    case kMenuDataDir: open_data_dir(); break;
    case kMenuQuit: request_quit(); break;
    default: break;
  }
}

// ---------------------------------------------------------------- 壁纸

bool WinApp::create_wallpaper_window() {
  if (wallpaper_hwnd_ && IsWindow(wallpaper_hwnd_)) return true;

  spawn_desktop_workerw();
  const RECT desktop = virtual_desktop_rect();
  const int width = desktop.right - desktop.left;
  const int height = desktop.bottom - desktop.top;

  HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kWallpaperClass, L"Fleeting Light", WS_POPUP,
                              desktop.left, desktop.top, width, height, nullptr, nullptr, instance_, this);
  if (!hwnd) {
    log_error("创建壁纸窗口失败: " + last_error_string());
    return false;
  }
  wallpaper_hwnd_ = hwnd;
  if (!mount_to_desktop(hwnd)) {
    log_error("挂载桌面底层失败，壁纸将不可见");
  }
  return true;
}

void WinApp::start_wallpaper() {
  if (quitting_) return;
  if (wallpaper_) {
    wallpaper_->destroy();
    wallpaper_.reset();
  }
  core_.detach_wallpaper();
  if (!create_wallpaper_window()) return;

  WebView2Surface::Options options;
  options.virtual_host = L"fleeting.light";
  options.web_root = to_wide(env::web_root());
  options.user_host = L"fleeting.local";
  options.user_root = to_wide(env::user_wallpapers_dir());
  options.inject_bridge = true;  // 导入的壁纸用不了 ../../shared 相对路径，靠注入
  options.dev_tools = core_.config().get_path("app.devTools").as_bool(false);
  options.background_argb = 0xFF05070C;

  wallpaper_ = std::make_shared<WebView2Surface>(wallpaper_hwnd_, std::move(options));
  core_.attach_wallpaper(wallpaper_.get());
}

void WinApp::destroy_wallpaper_window() {
  if (wallpaper_) {
    wallpaper_->destroy();
    wallpaper_.reset();
  }
  core_.detach_wallpaper();
  if (wallpaper_hwnd_) {
    HWND hwnd = wallpaper_hwnd_;
    wallpaper_hwnd_ = nullptr;
    DestroyWindow(hwnd);
  }
}

// ---------------------------------------------------------------- 设置窗口

void WinApp::open_settings() {
  if (settings_hwnd_ && IsWindow(settings_hwnd_)) {
    ShowWindow(settings_hwnd_, SW_RESTORE);
    SetWindowPos(settings_hwnd_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(settings_hwnd_);
    return;
  }

  RECT frame{0, 0, 1040, 720};
  AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);
  const int width = frame.right - frame.left;
  const int height = frame.bottom - frame.top;

  HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, kSettingsClass, L"Fleeting Light \u00B7 设置",
                              WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
                              nullptr, nullptr, instance_, this);
  if (!hwnd) {
    log_error("创建设置窗口失败: " + last_error_string());
    return;
  }
  center_window_on_cursor_monitor(hwnd, width, height);
  enable_dark_title_bar(hwnd);
  apply_rounded_corners(hwnd);

  settings_hwnd_ = hwnd;
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  // 从后台进程/托盘拉起窗口时，Windows 可能把它放在桌面（Progman）之下，
  // 这里显式提到 z 序最前并抢一次前台，否则用户会看不到面板。
  SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  SetForegroundWindow(hwnd);

  WebView2Surface::Options options;
  options.virtual_host = L"fleeting.light";
  options.web_root = to_wide(env::web_root());
  options.dev_tools = core_.config().get_path("app.devTools").as_bool(false);
  options.background_argb = 0xFF0B0D14;

  settings_ = std::make_shared<WebView2Surface>(hwnd, std::move(options));
  core_.on_settings_opened();
  core_.attach_settings(settings_.get());
}

void WinApp::close_settings() {
  if (settings_hwnd_ && IsWindow(settings_hwnd_)) DestroyWindow(settings_hwnd_);
}

// ---------------------------------------------------------------- IAppHost

void WinApp::set_wallpaper_suspended(bool suspended) {
  if (wallpaper_) wallpaper_->set_visible(!suspended);
}

void WinApp::set_autostart(bool enabled) {
  if (fl::win::set_autostart(enabled)) update_tooltip();
}

bool WinApp::autostart_enabled() const { return fl::win::autostart_enabled(); }

void WinApp::request_quit() {
  quitting_ = true;
  if (dispatcher_) PostMessageW(dispatcher_, WM_CLOSE, 0, 0);
}

void WinApp::open_data_dir() {
  const std::wstring dir = to_wide(env::user_data_dir());
  ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void WinApp::open_wallpaper_devtools() {
  if (wallpaper_) wallpaper_->open_devtools();
}

std::string WinApp::webview_version() const { return WebView2Env::instance().runtime_version(); }

std::string WinApp::pick_html_file() {
  HWND owner = (settings_hwnd_ && IsWindow(settings_hwnd_)) ? settings_hwnd_ : dispatcher_;
  return fl::win::pick_html_file(owner);
}

std::string WinApp::pick_folder() {
  HWND owner = (settings_hwnd_ && IsWindow(settings_hwnd_)) ? settings_hwnd_ : dispatcher_;
  return fl::win::pick_folder(owner);
}

void WinApp::request_import_wallpaper(bool folder) {
  if (dispatcher_) PostMessageW(dispatcher_, kImportWallpaperMessage, folder ? 1 : 0, 0);
}

void WinApp::poll_fullscreen() {
  const bool fullscreen = query_fullscreen_state();
  if (fullscreen == last_fullscreen_) return;
  last_fullscreen_ = fullscreen;
  log_info(fullscreen ? "检测到全屏应用" : "退出全屏");
  core_.on_fullscreen_changed(fullscreen);
}

}  // namespace fl::win
