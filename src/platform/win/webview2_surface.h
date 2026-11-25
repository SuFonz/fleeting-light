// IViewSurface 的 Windows 实现：Win32 窗口 + WebView2 控制器。
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <WebView2UUIDS.h>

#include "core/view_surface.h"
#include "platform/win/webview2_hooks.h"

namespace fl::win {

class WebView2Surface final : public core::IViewSurface,
                              public std::enable_shared_from_this<WebView2Surface> {
public:
    struct Options {
        std::wstring virtual_host = L"fleeting.light";
        std::wstring web_root;
        // 用户导入的壁纸放在另一个虚拟主机下，避免和内置资源混在一起
        std::wstring user_host = L"fleeting.local";
        std::wstring user_root;
        // 给导入的壁纸自动注入 bridge.js（它们拿不到 ../../shared 这类相对路径）
        bool inject_bridge            = false;
        bool dev_tools                = false;
        // 0xAARRGGBB：WebView2 默认背景色，避免首次加载白闪
        std::uint32_t background_argb = 0xFF0B0D14;
    };

    WebView2Surface(HWND hwnd, Options options);
    ~WebView2Surface() override;

    void create(const std::string& relative_url,
                MessageHandler on_message,
                std::function<void()> on_ready) override;
    void destroy() override;
    void navigate(const std::string& relative_url) override;
    void reload() override;
    void set_visible(bool visible) override;
    void send(std::string_view json) override;
    bool ready() const override {
        return ready_;
    }
    void open_devtools() override;

    void resize(int width, int height);
    bool attached() const {
        return webview_ != nullptr;
    }

private:
    void start();
    void on_environment_ready(HRESULT hr, ICoreWebView2Environment* environment);
    void on_controller_ready(HRESULT hr, ICoreWebView2Controller* controller);
    void configure_webview();
    void apply_bounds();
    void apply_background();
    void flush_pending();
    void release();
    std::wstring make_url(const std::string& relative_url) const;

    HWND hwnd_ = nullptr;
    Options options_;

    MessageHandler on_message_;
    std::function<void()> on_ready_;

    ComPtr<ICoreWebView2Environment> env_;
    ComPtr<ICoreWebView2Controller> controller_;
    ComPtr<ICoreWebView2> webview_;

    EventRegistrationToken token_message_{};
    EventRegistrationToken token_navigation_starting_{};
    EventRegistrationToken token_navigation_completed_{};
    EventRegistrationToken token_new_window_{};
    EventRegistrationToken token_process_failed_{};

    std::vector<std::string> pending_;
    std::string pending_url_;
    bool destroyed_       = false;
    bool ready_           = false;
    bool visible_         = true;
    bool start_requested_ = false;
    int width_            = 0;
    int height_           = 0;
};

}  // namespace fl::win
