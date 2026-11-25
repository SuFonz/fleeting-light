// WebView2 的 COM 胶水：
//   1) Handler<IIface, Args...>：把任意 WebView2 回调接口包成 std::function（这些接口的虚函数都叫 Invoke）
//   2) EnvironmentOptions：自行实现 ICoreWebView2EnvironmentOptions
//      （SDK 自带的 WebView2EnvironmentOptions.h 依赖 MSVC 的 <wrl/implements.h>，MinGW 用不了）
#pragma once

#include <cstring>
#include <functional>
#include <string>
#include <utility>

#include <WebView2UUIDS.h>
#include <wrl/client.h>

namespace fl::win {

using Microsoft::WRL::ComPtr;

template <typename TIface, typename... TArgs>
class Handler final : public TIface {
public:
    explicit Handler(std::function<HRESULT(TArgs...)> callback) : callback_(std::move(callback)) {
    }

    Handler(const Handler&)            = delete;
    Handler& operator=(const Handler&) = delete;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) noexcept override {
        if (!ppv) {
            return E_POINTER;
        }
        if (IsEqualIID(riid, __uuidof(TIface)) || IsEqualIID(riid, IID_IUnknown)) {
            *ppv = static_cast<TIface*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override {
        return static_cast<ULONG>(InterlockedIncrement(&ref_));
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override {
        const LONG count = InterlockedDecrement(&ref_);
        if (count == 0) {
            delete this;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE Invoke(TArgs... args) noexcept override {
        if (!callback_) {
            return S_OK;
        }
        return callback_(args...);
    }

private:
    LONG ref_ = 1;
    std::function<HRESULT(TArgs...)> callback_;
};

// 注意：形参不能直接写成 std::function<HRESULT(TArgs...)>。
// 当该形参由显式模板参数包替换得到时，clang 不会做 lambda -> std::function 的隐式转换
//（报 "could not match"），所以这里用泛型可调用对象接收，再显式构造 std::function。
template <typename TIface, typename... TArgs, typename TCallback>
ComPtr<TIface> make_handler(TCallback&& callback) {
    std::function<HRESULT(TArgs...)> function(std::forward<TCallback>(callback));
    return ComPtr<TIface>(new Handler<TIface, TArgs...>(std::move(function)));
}

// 极简 ICoreWebView2EnvironmentOptions 实现：只需要设置启动参数
class EnvironmentOptions final : public ICoreWebView2EnvironmentOptions {
public:
    explicit EnvironmentOptions(std::wstring additional_arguments)
        : arguments_(std::move(additional_arguments)) {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) noexcept override {
        if (!ppv) {
            return E_POINTER;
        }
        if (IsEqualIID(riid, __uuidof(ICoreWebView2EnvironmentOptions))
            || IsEqualIID(riid, IID_IUnknown)) {
            *ppv = static_cast<ICoreWebView2EnvironmentOptions*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override {
        return static_cast<ULONG>(InterlockedIncrement(&ref_));
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override {
        const LONG count = InterlockedDecrement(&ref_);
        if (count == 0) {
            delete this;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE get_AdditionalBrowserArguments(LPWSTR* value) noexcept override {
        return duplicate(arguments_, value);
    }
    HRESULT STDMETHODCALLTYPE put_AdditionalBrowserArguments(LPCWSTR value) noexcept override {
        arguments_ = value ? value : L"";
        return S_OK;
    }
    // 返回空字符串表示“跟随系统语言”；WebView2 会校验这个值，别乱塞
    HRESULT STDMETHODCALLTYPE get_Language(LPWSTR* value) noexcept override {
        return duplicate(language_, value);
    }
    HRESULT STDMETHODCALLTYPE put_Language(LPCWSTR value) noexcept override {
        language_ = value ? value : L"";
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_TargetCompatibleBrowserVersion(LPWSTR* value) noexcept override {
        return duplicate(target_version_, value);
    }
    HRESULT STDMETHODCALLTYPE put_TargetCompatibleBrowserVersion(LPCWSTR value) noexcept override {
        target_version_ = value ? value : L"";
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE
    get_AllowSingleSignOnUsingOSPrimaryAccount(BOOL* allow) noexcept override {
        if (!allow) {
            return E_POINTER;
        }
        *allow = allow_sso_ ? TRUE : FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE
    put_AllowSingleSignOnUsingOSPrimaryAccount(BOOL allow) noexcept override {
        allow_sso_ = allow == TRUE;
        return S_OK;
    }

private:
    // 空字符串要返回 nullptr（表示“不设置”），返回空串会被 WebView2 当成非法值
    static HRESULT duplicate(const std::wstring& source, LPWSTR* out) {
        if (!out) {
            return E_POINTER;
        }
        *out = nullptr;
        if (source.empty()) {
            return S_OK;
        }
        const std::size_t bytes = (source.size() + 1) * sizeof(wchar_t);
        auto buffer             = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));
        if (!buffer) {
            return E_OUTOFMEMORY;
        }
        std::memcpy(buffer, source.c_str(), bytes);
        *out = buffer;
        return S_OK;
    }

    LONG ref_ = 1;
    std::wstring arguments_;
    std::wstring language_;
    // 留空 = 不限制 WebView2 运行时版本（交给 loader 挑已安装的）
    std::wstring target_version_;
    bool allow_sso_ = false;
};

}  // namespace fl::win
