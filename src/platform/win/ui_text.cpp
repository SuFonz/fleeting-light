#include "platform/win/ui_text.h"

#include <windows.h>

#include <string>

#include "core/json.h"
#include "core/log.h"
#include "core/platform_env.h"
#include "platform/win/win_util.h"

namespace fl::win {
namespace {

// 整个 i18n.json
Json g_dictionary;
// 当前语言代码，例如 "zh" / "en"
std::string g_language = "en";

}  // namespace

void load_ui_text(const std::string& web_root) {
    const std::string path = env::join_path(web_root, "shared/i18n.json");

    std::string text;
    std::string error;
    if (!env::read_file(path, &text, &error)) {
        log_warn("无法读取界面文案: " + path + " (" + error + ")");
        return;
    }

    g_dictionary = Json::parse(text, &error);
    if (!g_dictionary.is_object()) {
        log_warn("界面文案格式不对: " + error);
        g_dictionary = Json::object();
        return;
    }

    log_info("界面文案已加载: " + path);
}

void set_ui_language(const std::string& setting) {
    // 只有 auto 才去问系统，其余直接认
    if (setting == "zh" || setting == "en") {
        g_language = setting;
        return;
    }

    const LANGID id = GetUserDefaultUILanguage();
    g_language = (PRIMARYLANGID(id) == LANG_CHINESE) ? "zh" : "en";
}

std::string ui_language() {
    return g_language;
}

std::wstring tr(const char* key) {
    if (key == nullptr) {
        return std::wstring();
    }

    if (g_dictionary.is_object() && g_dictionary.contains(key)) {
        const Json& entry = g_dictionary.at(key);
        if (entry.is_object() && entry.contains(g_language)) {
            return to_wide(entry.at(g_language).as_string(key));
        }
    }

    // 没查到就把 key 显示出来，方便发现遗漏
    return to_wide(key);
}

}  // namespace fl::win
