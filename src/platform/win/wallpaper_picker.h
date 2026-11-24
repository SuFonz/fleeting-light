// 系统选择器：选单个 HTML 文件，或选一个壁纸文件夹。
// 优先用 IFileOpenDialog（现代对话框），失败时退回 GetOpenFileName / SHBrowseForFolder。
#pragma once

#include <string>

#include "platform/win/win_util.h"

namespace fl::win {

// 返回 UTF-8 绝对路径；用户取消返回空字符串
std::string pick_html_file(HWND owner);
std::string pick_folder(HWND owner);

}  // namespace fl::win
