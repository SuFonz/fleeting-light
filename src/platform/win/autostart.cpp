#include "platform/win/autostart.h"

#include <vector>

#include "core/log.h"
#include "platform/win/win_util.h"

namespace fl::win {
namespace {

constexpr const wchar_t* kRunKey    = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kValueName = L"FleetingLight";

bool read_value(std::wstring* out) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type  = 0;
    DWORD bytes = 0;
    LONG result = RegQueryValueExW(key, kValueName, nullptr, &type, nullptr, &bytes);
    bool ok     = false;
    if (result == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && bytes > 0) {
        std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
        result = RegQueryValueExW(
            key, kValueName, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &bytes);
        if (result == ERROR_SUCCESS) {
            if (out) {
                *out = std::wstring(buffer.data());
            }
            ok = true;
        }
    }
    RegCloseKey(key);
    return ok;
}

}  // namespace

std::wstring autostart_command() {
    return L"\"" + module_path() + L"\" --autostart";
}

bool set_autostart(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr)
        != ERROR_SUCCESS) {
        log_warn("无法写入开机自启注册表");
        return false;
    }
    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring command = autostart_command();
        result = RegSetValueExW(key,
                                kValueName,
                                0,
                                REG_SZ,
                                reinterpret_cast<const BYTE*>(command.c_str()),
                                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kValueName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        log_warn("更新开机自启失败: " + last_error_string(static_cast<DWORD>(result)));
        return false;
    }
    log_info(enabled ? "已开启开机自启" : "已关闭开机自启");
    return true;
}

bool autostart_enabled() {
    std::wstring value;
    if (!read_value(&value)) {
        return false;
    }
    return !value.empty();
}

}  // namespace fl::win
