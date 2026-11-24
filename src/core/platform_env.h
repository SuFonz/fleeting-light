// 平台环境能力（由各平台后端实现）：路径、文件读写、应用信息。
// 上层 core 只依赖这里的声明，不直接碰 Win32 / POSIX。
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace fl::env {

enum class FileMode { Read, Write, Append };

// 可执行文件所在目录（UTF-8，无结尾分隔符）
std::string exe_dir();

// 用户数据目录（配置/日志/WebView2 缓存），调用即保证存在
std::string user_data_dir();

// 用户导入的壁纸根目录：<user_data_dir>/wallpapers
std::string user_wallpapers_dir();

// 前端资源根目录：默认 <exe_dir>/web，可用环境变量 FL_WEB_ROOT 覆盖（开发方便）
std::string web_root();

// 应用版本号
std::string app_version();

std::string join_path(std::string_view a, std::string_view b);
std::string base_name(std::string_view path);            // 去掉目录部分
std::string file_stem(std::string_view path);            // 再去掉扩展名
bool file_exists(std::string_view path);
bool dir_exists(std::string_view path);
bool ensure_dir(std::string_view path);

bool read_file(std::string_view path, std::string* out, std::string* error);
bool write_file(std::string_view path, std::string_view data, FileMode mode, std::string* error);

// 列出目录下的子目录名（不含 . / ..）
bool list_dirs(std::string_view path, std::vector<std::string>* out, std::string* error);

bool copy_file(std::string_view from, std::string_view to, std::string* error);
bool copy_tree(std::string_view from, std::string_view to, std::string* error);
bool remove_tree(std::string_view path, std::string* error);

}  // namespace fl::env
