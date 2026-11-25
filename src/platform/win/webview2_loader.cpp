#include "platform/win/webview2_loader.h"

#include <vector>

#include "core/log.h"
#include "core/platform_env.h"
#include "platform/win/win_util.h"

namespace fl::win {

WebView2Loader& WebView2Loader::instance() {
    static WebView2Loader loader;
    return loader;
}

bool WebView2Loader::load(std::string* error) {
    if (module_) {
        return true;
    }

    std::vector<std::wstring> candidates;
    candidates.push_back(module_dir() + L"\\WebView2Loader.dll");
    candidates.push_back(L"WebView2Loader.dll");
    candidates.push_back(module_dir()
                         + L"\\..\\third_party\\webview2\\bin\\x64\\WebView2Loader.dll");

    for (const std::wstring& path : candidates) {
        module_ = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (module_) {
            log_info("WebView2Loader 已加载: " + to_utf8(path));
            break;
        }
    }
    if (!module_) {
        // 最后再试一次普通搜索（已在系统 PATH 中的情况）
        module_ = LoadLibraryW(L"WebView2Loader.dll");
    }
    if (!module_) {
        if (error) {
            *error = "无法加载 WebView2Loader.dll（" + last_error_string() + "）";
        }
        return false;
    }

    create_environment = reinterpret_cast<CreateCoreWebView2EnvironmentFn>(
        GetProcAddress(module_, "CreateCoreWebView2EnvironmentWithOptions"));
    browser_version = reinterpret_cast<GetAvailableCoreWebView2BrowserVersionFn>(
        GetProcAddress(module_, "GetAvailableCoreWebView2BrowserVersionString"));
    if (!create_environment || !browser_version) {
        if (error) {
            *error = "WebView2Loader.dll 导出函数缺失，SDK 与加载器版本可能不匹配";
        }
        return false;
    }
    return true;
}

}  // namespace fl::win
