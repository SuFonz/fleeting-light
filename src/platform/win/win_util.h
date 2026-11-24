// Win32 小工具：字符串转换、错误格式化、暗色标题栏等。
#pragma once

#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace fl::win {

std::wstring to_wide(std::string_view utf8);
std::string to_utf8(std::wstring_view wide);
std::string format_hresult(HRESULT hr);
std::string last_error_string(DWORD code = 0);

std::wstring module_path();
std::wstring module_dir();

bool is_windows_11();
void enable_dark_title_bar(HWND hwnd);
void apply_rounded_corners(HWND hwnd);

// 4:3 -> 16:9 之类的小工具，给设置窗口居中用
void center_window_on_cursor_monitor(HWND hwnd, int width, int height);

}  // namespace fl::win
