#include "platform/win/win_util.h"

#include <dwmapi.h>
#include <shellscalingapi.h>

#include <cstdio>
#include <vector>

namespace fl::win {

std::wstring to_wide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int size =
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), size);
    return out;
}

std::string to_utf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::string format_hresult(HRESULT hr) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "0x%08lX", static_cast<unsigned long>(hr));
    return buffer;
}

std::string last_error_string(DWORD code) {
    if (code == 0) {
        code = GetLastError();
    }
    LPWSTR buffer = nullptr;
    const DWORD flags =
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = FormatMessageW(flags,
                                      nullptr,
                                      code,
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPWSTR>(&buffer),
                                      0,
                                      nullptr);
    std::string text = "错误 " + std::to_string(code);
    if (size && buffer) {
        std::wstring wide(buffer, size);
        while (!wide.empty()
               && (wide.back() == L'\r' || wide.back() == L'\n' || wide.back() == L' ')) {
            wide.pop_back();
        }
        text += ": " + to_utf8(wide);
    }
    if (buffer) {
        LocalFree(buffer);
    }
    return text;
}

std::wstring module_path() {
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        const DWORD size =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (size == 0) {
            return {};
        }
        if (size < buffer.size()) {
            return std::wstring(buffer.data(), size);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring module_dir() {
    std::wstring path       = module_path();
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

bool is_windows_11() {
    static const bool result = [] {
        RTL_OSVERSIONINFOW info{};
        info.dwOSVersionInfoSize = sizeof(info);
        // RtlGetVersion 不走兼容性垫片，拿到的才是真实版本
        using RtlGetVersionFn    = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        HMODULE ntdll            = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) {
            return false;
        }
        auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
        if (!fn || fn(reinterpret_cast<PRTL_OSVERSIONINFOW>(&info)) != 0) {
            return false;
        }
        return info.dwMajorVersion >= 10 && info.dwBuildNumber >= 22000;
    }();
    return result;
}

void enable_dark_title_bar(HWND hwnd) {
    BOOL dark                                  = TRUE;
    constexpr DWORD kDwmwaUseImmersiveDarkMode = 20;
    if (FAILED(DwmSetWindowAttribute(hwnd, kDwmwaUseImmersiveDarkMode, &dark, sizeof(dark)))) {
        constexpr DWORD kDwmwaUseImmersiveDarkModeOld = 19;
        DwmSetWindowAttribute(hwnd, kDwmwaUseImmersiveDarkModeOld, &dark, sizeof(dark));
    }
}

void apply_rounded_corners(HWND hwnd) {
    if (!is_windows_11()) {
        return;
    }
    constexpr DWORD kDwmwaWindowCornerPreference = 33;
    constexpr int kRound                         = 2;
    DwmSetWindowAttribute(hwnd, kDwmwaWindowCornerPreference, &kRound, sizeof(kRound));
}

void center_window_on_cursor_monitor(HWND hwnd, int width, int height) {
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return;
    }
    const RECT work = info.rcWork;
    const int x     = work.left + ((work.right - work.left) - width) / 2;
    const int y     = work.top + ((work.bottom - work.top) - height) / 2;
    SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace fl::win
