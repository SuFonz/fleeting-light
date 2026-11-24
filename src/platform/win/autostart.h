// 开机自启：写 HKCU\...\CurrentVersion\Run
#pragma once

#include <string>

namespace fl::win {
bool set_autostart(bool enabled);
bool autostart_enabled();
// 当前应写入注册表的命令行（不含引号处理）
std::wstring autostart_command();

}  // namespace fl::win
