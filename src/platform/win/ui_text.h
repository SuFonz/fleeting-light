// 原生界面的文案（托盘菜单、提示气泡、窗口标题、文件对话框）。
//
// 文案不在代码里：统一从 web/shared/i18n.json 读，和设置面板共用同一份数据。
// 加一门语言只需要在那个文件里给每条加一列，这里一行都不用改。
//
// 用法：tr("tray.settings")  —— 直接返回按当前语言翻译好的宽字符串。
// 缺 key 时返回 key 本身，方便一眼看出漏了什么。

#pragma once

#include <string>

namespace fl::win {

// 启动时读一次字典（web_root 是前端资源根目录）
void load_ui_text(const std::string& web_root);

// setting: "auto" / "zh" / "en"（"auto" 时跟随系统 UI 语言）
void set_ui_language(const std::string& setting);

// 当前实际生效的语言代码
std::string ui_language();

// 按 key 取文案；找不到就原样返回 key
std::wstring tr(const char* key);

}  // namespace fl::win
