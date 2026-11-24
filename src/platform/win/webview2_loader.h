// WebView2Loader.dll 的动态加载封装。
// 不走 import lib：MinGW 链接 MSVC 的 .lib 容易出事，直接 LoadLibrary + GetProcAddress 最稳，
// 顺便还能在运行时缺失时给出友好提示。
#pragma once

#include <string>

#include <WebView2UUIDS.h>

namespace fl::win {

using CreateCoreWebView2EnvironmentFn = HRESULT(WINAPI*)(PCWSTR browser_executable_folder, PCWSTR user_data_folder,
                                                         ICoreWebView2EnvironmentOptions* options,
                                                         ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* handler);
using GetAvailableCoreWebView2BrowserVersionFn = HRESULT(WINAPI*)(PCWSTR browser_executable_folder, LPWSTR* version);

class WebView2Loader {
 public:
  static WebView2Loader& instance();

  // 依次尝试：exe 目录 -> 系统搜索路径 -> 仓库内 third_party 目录（开发用）
  bool load(std::string* error);
  bool loaded() const { return module_ != nullptr; }

  CreateCoreWebView2EnvironmentFn create_environment = nullptr;
  GetAvailableCoreWebView2BrowserVersionFn browser_version = nullptr;

 private:
  HMODULE module_ = nullptr;
};

}  // namespace fl::win
