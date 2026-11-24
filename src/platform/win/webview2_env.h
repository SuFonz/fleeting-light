// 全进程共享的单一 CoreWebView2Environment。
// 壁纸视图和设置面板都从它创建 CoreWebView2Controller，共享同一个浏览器进程与缓存目录。
#pragma once

#include <functional>
#include <string>
#include <vector>

#include <WebView2UUIDS.h>

#include "platform/win/webview2_hooks.h"

namespace fl::win {

class WebView2Env {
 public:
  using ReadyCallback = std::function<void(HRESULT, ICoreWebView2Environment*)>;

  static WebView2Env& instance();

  // 载入 WebView2Loader.dll 并查询运行时版本
  bool initialize(std::string* error);
  const std::string& runtime_version() const { return runtime_version_; }

  // 幂等：环境就绪后立刻回调，创建中则排队
  void ensure(ReadyCallback callback);

  ICoreWebView2Environment* environment() const { return env_.Get(); }
  bool ready() const { return env_ != nullptr; }

  void set_user_data_folder(std::wstring folder) { user_data_folder_ = std::move(folder); }
  void set_browser_arguments(std::wstring arguments) { browser_arguments_ = std::move(arguments); }

 private:
  WebView2Env() = default;
  void create();

  ComPtr<ICoreWebView2Environment> env_;
  std::vector<ReadyCallback> waiters_;
  bool creating_ = false;
  std::string runtime_version_;
  std::wstring user_data_folder_;
  std::wstring browser_arguments_;
};

}  // namespace fl::win
