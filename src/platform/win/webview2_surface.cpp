#include "platform/win/webview2_surface.h"

#include <algorithm>

#include "core/log.h"
#include "core/platform_env.h"
#include "platform/win/webview2_env.h"
#include "platform/win/win_util.h"

namespace fl::win {
namespace {

constexpr std::size_t kMaxPendingMessages = 64;

}  // namespace

WebView2Surface::WebView2Surface(HWND hwnd, Options options)
    : hwnd_(hwnd), options_(std::move(options)) {
    if (hwnd_) {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        width_  = rc.right;
        height_ = rc.bottom;
    }
}

WebView2Surface::~WebView2Surface() {
    release();
}

void WebView2Surface::release() {
    destroyed_ = true;
    if (webview_ && token_message_.value != 0) {
        webview_->remove_WebMessageReceived(token_message_);
    }
    token_message_ = {};
    webview_.Reset();
    if (controller_) {
        controller_->Close();
        controller_.Reset();
    }
    env_.Reset();
    pending_.clear();
}

void WebView2Surface::destroy() {
    release();
}

void WebView2Surface::create(const std::string& relative_url,
                             MessageHandler on_message,
                             std::function<void()> on_ready) {
    on_message_  = std::move(on_message);
    on_ready_    = std::move(on_ready);
    pending_url_ = relative_url;
    destroyed_   = false;
    start();
}

void WebView2Surface::start() {
    if (start_requested_) {
        return;
    }
    if (!hwnd_ || !IsWindow(hwnd_)) {
        log_error("WebView2Surface: 父窗口无效，无法创建视图");
        return;
    }
    start_requested_                    = true;
    std::weak_ptr<WebView2Surface> weak = weak_from_this();
    WebView2Env::instance().ensure([weak](HRESULT hr, ICoreWebView2Environment* environment) {
        if (auto self = weak.lock()) {
            self->on_environment_ready(hr, environment);
        }
    });
}

void WebView2Surface::on_environment_ready(HRESULT hr, ICoreWebView2Environment* environment) {
    if (destroyed_ || !hwnd_ || !IsWindow(hwnd_)) {
        return;
    }
    if (FAILED(hr) || !environment) {
        log_error("WebView2Surface: 环境不可用: " + format_hresult(hr));
        return;
    }
    env_ = environment;

    std::weak_ptr<WebView2Surface> weak = weak_from_this();
    auto handler = make_handler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                                HRESULT,
                                ICoreWebView2Controller*>(
        [weak](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
            if (auto self = weak.lock()) {
                self->on_controller_ready(result, controller);
            }
            return S_OK;
        });

    const HRESULT result = env_->CreateCoreWebView2Controller(hwnd_, handler.Get());
    if (FAILED(result)) {
        log_error("WebView2Surface: CreateCoreWebView2Controller 失败: " + format_hresult(result));
        start_requested_ = false;
    }
}

void WebView2Surface::on_controller_ready(HRESULT hr, ICoreWebView2Controller* controller) {
    if (destroyed_ || !controller) {
        log_error("WebView2Surface: 控制器创建失败: " + format_hresult(hr));
        start_requested_ = false;
        return;
    }
    controller_ = controller;
    apply_background();
    apply_bounds();
    controller_->put_IsVisible(visible_ ? TRUE : FALSE);

    if (FAILED(controller_->get_CoreWebView2(&webview_)) || !webview_) {
        log_error("WebView2Surface: 无法取得 CoreWebView2");
        return;
    }
    configure_webview();
    if (!pending_url_.empty()) {
        navigate(pending_url_);
        pending_url_.clear();
    }
}

void WebView2Surface::apply_background() {
    if (!controller_) {
        return;
    }
    ComPtr<ICoreWebView2Controller2> controller2;
    if (FAILED(controller_.As(&controller2)) || !controller2) {
        return;
    }
    COREWEBVIEW2_COLOR color{};
    color.A = static_cast<BYTE>((options_.background_argb >> 24) & 0xFF);
    color.R = static_cast<BYTE>((options_.background_argb >> 16) & 0xFF);
    color.G = static_cast<BYTE>((options_.background_argb >> 8) & 0xFF);
    color.B = static_cast<BYTE>(options_.background_argb & 0xFF);
    controller2->put_DefaultBackgroundColor(color);
}

void WebView2Surface::apply_bounds() {
    if (!controller_) {
        return;
    }
    RECT bounds{0, 0, width_, height_};
    if (bounds.right <= 0 || bounds.bottom <= 0) {
        return;
    }
    controller_->put_Bounds(bounds);
}

void WebView2Surface::configure_webview() {
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webview_->get_Settings(&settings)) && settings) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
        settings->put_AreDevToolsEnabled(options_.dev_tools ? TRUE : FALSE);
        settings->put_IsBuiltInErrorPageEnabled(FALSE);
    }
    // AreBrowserAcceleratorKeysEnabled 在 Settings3 上（F5/Ctrl+R 之类交给我们自己处理）
    ComPtr<ICoreWebView2Settings3> settings3;
    if (settings && SUCCEEDED(settings.As(&settings3)) && settings3) {
        settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
    }

    // 虚拟主机映射：把本地目录挂成 https 源，ES module / fetch 才能正常工作
    ComPtr<ICoreWebView2_3> webview3;
    if (SUCCEEDED(webview_.As(&webview3)) && webview3) {
        const auto map_host = [&webview3](const std::wstring& host, const std::wstring& folder) {
            if (folder.empty()) {
                return;
            }
            const HRESULT hr = webview3->SetVirtualHostNameToFolderMapping(
                host.c_str(), folder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
            if (FAILED(hr)) {
                log_error("WebView2Surface: 虚拟主机映射失败: " + format_hresult(hr)
                          + " host=" + to_utf8(host) + " dir=" + to_utf8(folder));
            }
        };
        map_host(options_.virtual_host, options_.web_root);
        map_host(options_.user_host, options_.user_root);
    }

    // 导入的壁纸是独立页面，拿不到 ../../shared/bridge.js，这里直接把桥接注入进去
    if (options_.inject_bridge) {
        std::string script;
        const std::string bridge_path =
            env::join_path(env::join_path(env::web_root(), "shared"), "bridge.js");
        if (env::read_file(bridge_path, &script, nullptr) && !script.empty()) {
            webview_->AddScriptToExecuteOnDocumentCreated(to_wide(script).c_str(), nullptr);
        } else {
            log_warn("读取 bridge.js 失败，导入的壁纸将无法使用 bridge 接口: " + bridge_path);
        }
    }

    std::weak_ptr<WebView2Surface> weak = weak_from_this();

    auto message_handler = make_handler<ICoreWebView2WebMessageReceivedEventHandler,
                                        ICoreWebView2*,
                                        ICoreWebView2WebMessageReceivedEventArgs*>(
        [weak](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
            LPWSTR raw = nullptr;
            if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
                const std::string json = to_utf8(raw);
                CoTaskMemFree(raw);
                if (auto self = weak.lock(); self && self->on_message_) {
                    self->on_message_(json);
                }
            }
            return S_OK;
        });
    webview_->add_WebMessageReceived(message_handler.Get(), &token_message_);

    auto starting_handler = make_handler<ICoreWebView2NavigationStartingEventHandler,
                                         ICoreWebView2*,
                                         ICoreWebView2NavigationStartingEventArgs*>(
        [weak](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*) -> HRESULT {
            if (auto self = weak.lock()) {
                self->ready_ = false;
            }
            return S_OK;
        });
    webview_->add_NavigationStarting(starting_handler.Get(), &token_navigation_starting_);

    auto completed_handler = make_handler<ICoreWebView2NavigationCompletedEventHandler,
                                          ICoreWebView2*,
                                          ICoreWebView2NavigationCompletedEventArgs*>(
        [weak](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
            BOOL success = FALSE;
            args->get_IsSuccess(&success);
            if (auto self = weak.lock()) {
                if (!success) {
                    COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                    args->get_WebErrorStatus(&status);
                    log_error("WebView2Surface: 页面加载失败 (status "
                              + std::to_string(static_cast<int>(status))
                              + "): " + to_utf8(self->make_url(self->pending_url_)));
                }
                self->ready_ = true;
                self->flush_pending();
                if (self->on_ready_) {
                    self->on_ready_();
                }
            }
            return S_OK;
        });
    webview_->add_NavigationCompleted(completed_handler.Get(), &token_navigation_completed_);

    auto new_window_handler = make_handler<ICoreWebView2NewWindowRequestedEventHandler,
                                           ICoreWebView2*,
                                           ICoreWebView2NewWindowRequestedEventArgs*>(
        [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
            args->put_Handled(TRUE);
            return S_OK;
        });
    webview_->add_NewWindowRequested(new_window_handler.Get(), &token_new_window_);

    auto failed_handler = make_handler<ICoreWebView2ProcessFailedEventHandler,
                                       ICoreWebView2*,
                                       ICoreWebView2ProcessFailedEventArgs*>(
        [weak](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
            COREWEBVIEW2_PROCESS_FAILED_KIND kind =
                COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;
            args->get_ProcessFailedKind(&kind);
            log_error("WebView2Surface: 渲染进程异常 (kind "
                      + std::to_string(static_cast<int>(kind)) + ")，尝试重新加载");
            if (auto self = weak.lock(); self && self->webview_) {
                self->webview_->Reload();
            }
            return S_OK;
        });
    webview_->add_ProcessFailed(failed_handler.Get(), &token_process_failed_);
}

std::wstring WebView2Surface::make_url(const std::string& relative_url) const {
    // 已经是绝对地址（用户导入的壁纸走第二个虚拟主机）就原样使用
    if (relative_url.rfind("http://", 0) == 0 || relative_url.rfind("https://", 0) == 0) {
        return to_wide(relative_url);
    }
    std::wstring path = to_wide(relative_url);
    while (!path.empty() && (path.front() == L'/' || path.front() == L'\\')) {
        path.erase(path.begin());
    }
    return L"https://" + options_.virtual_host + L"/" + path;
}

void WebView2Surface::navigate(const std::string& relative_url) {
    pending_url_ = relative_url;
    if (!webview_) {
        return;
    }
    const std::wstring url = make_url(relative_url);
    webview_->Navigate(url.c_str());
}

void WebView2Surface::reload() {
    if (webview_) {
        webview_->Reload();
    }
}

void WebView2Surface::set_visible(bool visible) {
    if (visible_ == visible) {
        if (visible) {
            apply_bounds();
        }
        return;
    }
    visible_ = visible;
    if (!controller_) {
        return;
    }
    controller_->put_IsVisible(visible ? TRUE : FALSE);
    if (visible) {
        // 从隐藏恢复时尺寸通常没变，apply_bounds 会提前 return，
        // 但 WebView2 需要一次 bounds 变化才会重新提交合成帧，所以这里强制塞一次。
        RECT zero{0, 0, 0, 0};
        controller_->put_Bounds(zero);
        apply_bounds();
    }
}

void WebView2Surface::resize(int width, int height) {
    if (width == width_ && height == height_) {
        return;
    }
    width_  = width;
    height_ = height;
    apply_bounds();
}

void WebView2Surface::send(std::string_view json) {
    if (destroyed_) {
        return;
    }
    if (!ready_ || !webview_) {
        if (pending_.size() < kMaxPendingMessages) {
            pending_.emplace_back(json);
        }
        log_debug("send: 未就绪，入队 " + std::to_string(pending_.size()));
        return;
    }
    const std::wstring wide = to_wide(json);
    const HRESULT hr        = webview_->PostWebMessageAsString(wide.c_str());
    if (FAILED(hr)) {
        log_warn("PostWebMessageAsString 失败: " + format_hresult(hr)
                 + " payload=" + std::string(json.substr(0, 80)));
    }
}

void WebView2Surface::flush_pending() {
    if (!webview_) {
        return;
    }
    std::vector<std::string> pending;
    pending.swap(pending_);
    for (const std::string& message : pending) {
        const std::wstring wide = to_wide(message);
        webview_->PostWebMessageAsString(wide.c_str());
    }
}

void WebView2Surface::open_devtools() {
    if (!webview_) {
        return;
    }
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webview_->get_Settings(&settings)) && settings) {
        settings->put_AreDevToolsEnabled(TRUE);
    }
    webview_->OpenDevToolsWindow();
}

}  // namespace fl::win
