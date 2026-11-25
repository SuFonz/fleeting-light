// 核心编排层（平台无关）：配置、状态机、消息路由、省电策略、壁纸导入。
// 平台后端只需实现 IAppHost（开窗/托盘/挂载桌面/文件选择…）和 IViewSurface（网页视图）。
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/config.h"
#include "core/json.h"
#include "core/view_surface.h"

namespace fl::core {

// 平台宿主能力
class IAppHost {
public:
    virtual ~IAppHost() = default;

    virtual void open_settings()       = 0;
    virtual void close_settings()      = 0;
    virtual bool settings_open() const = 0;

    // 真正把壁纸视图藏起来/放出来（省 GPU）；user 暂停时不需要隐藏
    virtual void set_wallpaper_suspended(bool suspended) = 0;

    virtual void set_autostart(bool enabled) = 0;
    virtual bool autostart_enabled() const   = 0;

    virtual void update_tray()                  = 0;
    virtual void request_quit()                 = 0;
    virtual void open_data_dir()                = 0;
    virtual void open_wallpaper_devtools()      = 0;
    virtual std::string platform_name() const   = 0;
    virtual std::string webview_version() const = 0;

    // 两个网页源：内置资源 / 用户导入的壁纸
    virtual std::string builtin_host() const = 0;
    virtual std::string user_host() const    = 0;

    // 系统选择器；用户取消返回空串
    virtual std::string pick_html_file()               = 0;
    virtual std::string pick_folder()                  = 0;
    // 延后到消息循环里再弹模态框，避免在 WebView2 回调栈里开嵌套对话框
    virtual void request_import_wallpaper(bool folder) = 0;
};

struct WallpaperEntry {
    std::string id;  // 不含 "user:" 前缀
    std::string name;
    std::string description;
    // 预览图相对路径（相对 web 根目录，例如 "wallpapers/riverside/preview.jpg"）。
    // 为空表示没有预览图，设置面板会退回到图标占位。
    std::string preview;
    bool user = false;

    std::string ref() const {
        return user ? "user:" + id : id;
    }
};

class App {
public:
    explicit App(IAppHost& host);
    ~App();

    void start();

    // 平台在视图创建/销毁时挂接
    void attach_wallpaper(IViewSurface* surface);
    void detach_wallpaper();
    void attach_settings(IViewSurface* surface);
    void detach_settings();

    // ---- 平台事件 ----
    void on_fullscreen_changed(bool fullscreen);
    void on_locked(bool locked);
    void on_system_suspend(bool suspended);
    void on_settings_opened();
    void on_settings_closed();

    // ---- 托盘/快捷键入口 ----
    void toggle_wallpaper_enabled();
    void set_wallpaper_enabled(bool enabled);
    void reload_wallpaper();
    void reset_config();

    // ---- 壁纸导入（由平台在选择器返回后调用）----
    void import_wallpaper(bool folder);
    void remove_wallpaper(std::string_view ref);

    // 桌面互动：平台定时轮询光标后调用，坐标已归一化到壁纸视图（0..1）
    void on_pointer(float x, float y, bool inside, bool down);
    bool wallpaper_interaction_enabled() const;

    const Json& config() const {
        return config_.data();
    }
    const std::string& config_path() const;
    bool paused() const {
        return state_paused_;
    }
    bool enabled() const {
        return !in_disabled_;
    }
    const std::string& pause_reason() const {
        return pause_reason_;
    }
    std::string wallpaper_url() const;
    std::vector<WallpaperEntry> wallpapers() const {
        return wallpapers_;
    }
    std::string wallpaper_display_name(std::string_view ref) const;

private:
    void load_wallpaper_manifest();
    void import_video_wallpaper(const std::string& path);
    std::string default_wallpaper_ref() const;
    void recompute_state();
    void apply_suspension();
    void broadcast_config();
    void broadcast_state();
    void push_wallpaper_init();
    void push_settings_init();
    void push_settings_toast(const char* level, std::string message);
    void switch_wallpaper(std::string ref);

    void handle_wallpaper_message(std::string_view json);
    void handle_settings_message(std::string_view json);
    void handle_action(std::string_view name, const Json& payload);

    IAppHost& host_;
    ConfigCenter config_;

    IViewSurface* wallpaper_ = nullptr;
    IViewSurface* settings_  = nullptr;

    std::vector<WallpaperEntry> wallpapers_;

    // 输入状态
    bool in_disabled_      = false;
    bool in_locked_        = false;
    bool in_fullscreen_    = false;
    bool in_suspend_       = false;
    bool in_settings_open_ = false;

    // 输出状态
    bool state_paused_ = false;
    std::string pause_reason_;
    bool suspended_ = false;
    bool started_   = false;

    // 上一次推给壁纸的光标状态（用来去抖，避免每帧都发）
    float pointer_x_     = -1.0f;
    float pointer_y_     = -1.0f;
    bool pointer_inside_ = false;
    bool pointer_down_   = false;
};

}  // namespace fl::core
