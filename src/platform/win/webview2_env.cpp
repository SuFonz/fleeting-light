#include "platform/win/webview2_env.h"

#include "core/log.h"
#include "core/platform_env.h"
#include "platform/win/webview2_loader.h"
#include "platform/win/win_util.h"

namespace fl::win {

WebView2Env& WebView2Env::instance() {
  static WebView2Env env;
  return env;
}

bool WebView2Env::initialize(std::string* error) {
  if (!WebView2Loader::instance().load(error)) return false;

  LPWSTR version = nullptr;
  const HRESULT hr = WebView2Loader::instance().browser_version(nullptr, &version);
  if (FAILED(hr) || !version) {
    if (error) *error = "未检测到 WebView2 运行时（HRESULT " + format_hresult(hr) + "）";
    return false;
  }
  runtime_version_ = to_utf8(version);
  CoTaskMemFree(version);
  log_info("WebView2 运行时版本: " + runtime_version_);
  return true;
}

void WebView2Env::ensure(ReadyCallback callback) {
  if (env_) {
    callback(S_OK, env_.Get());
    return;
  }
  waiters_.push_back(std::move(callback));
  if (creating_) return;
  creating_ = true;
  create();
}

void WebView2Env::create() {
  if (!WebView2Loader::instance().loaded()) {
    std::string error;
    if (!WebView2Loader::instance().load(&error)) {
      log_error("创建环境失败: " + error);
      auto waiters = std::move(waiters_);
      waiters_.clear();
      creating_ = false;
      for (auto& waiter : waiters) waiter(E_FAIL, nullptr);
      return;
    }
  }

  if (user_data_folder_.empty()) {
    user_data_folder_ = to_wide(env::join_path(env::user_data_dir(), "WebView2"));
  }

  ComPtr<ICoreWebView2EnvironmentOptions> options(new EnvironmentOptions(browser_arguments_));
  // TargetCompatibleBrowserVersion 必须是一个“已安装运行时满足”的版本号：
  //   传空  -> WebView2 直接返回 E_INVALIDARG（实测）
  //   传 SDK 默认的 CORE_WEBVIEW_TARGET_PRODUCT_VERSION(158.x) -> 老运行时(157.x) 上返回 ERROR_FILE_NOT_FOUND
  //   传比运行时更高的版本号同样失败
  // 所以这里直接用本机已安装的版本，兼容性最好。
  if (!runtime_version_.empty()) {
    options->put_TargetCompatibleBrowserVersion(to_wide(runtime_version_).c_str());
  }

  auto handler = make_handler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment*>(
      [this](HRESULT hr, ICoreWebView2Environment* environment) -> HRESULT {
        creating_ = false;
        if (SUCCEEDED(hr) && environment) {
          env_ = environment;
          log_info("CoreWebView2Environment 就绪");
        } else {
          log_error("创建 CoreWebView2Environment 失败: " + format_hresult(hr));
        }
        auto waiters = std::move(waiters_);
        waiters_.clear();
        for (auto& waiter : waiters) waiter(hr, env_.Get());
        return S_OK;
      });

  const HRESULT hr = WebView2Loader::instance().create_environment(nullptr, user_data_folder_.c_str(), options.Get(),
                                                                   handler.Get());
  if (FAILED(hr)) {
    log_error("CreateCoreWebView2EnvironmentWithOptions 调用失败: " + format_hresult(hr));
    creating_ = false;
    auto waiters = std::move(waiters_);
    waiters_.clear();
    for (auto& waiter : waiters) waiter(hr, nullptr);
  }
}

}  // namespace fl::win
