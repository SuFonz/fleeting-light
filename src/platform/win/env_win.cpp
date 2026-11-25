// fl::env 的 Windows 实现：全部走宽字符 API，保证中文路径可用。
#include <shlobj.h>

#include <string>
#include <vector>

#include "core/platform_env.h"
#include "platform/win/win_util.h"

#ifndef FL_VERSION
#define FL_VERSION "0.0.0"
#endif

namespace fl::env {
namespace {

std::wstring known_folder_local_appdata() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)) && path) {
        std::wstring out(path);
        CoTaskMemFree(path);
        return out;
    }
    return {};
}

std::wstring utf8_path(std::string_view path) {
    return win::to_wide(path);
}

bool create_dir_w(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    const DWORD error = GetLastError();
    return error == ERROR_ALREADY_EXISTS;
}

bool ensure_dir_w(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    std::wstring nested;
    for (wchar_t c : path) {
        nested.push_back(c);
        if ((c == L'\\' || c == L'/') && nested.size() > 3) {
            create_dir_w(nested);
        }
    }
    return create_dir_w(path);
}

bool remove_tree_w(const std::wstring& path, std::string* error) {
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((path + L"\\*").c_str(), &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
                continue;
            }
            const std::wstring child = path + L"\\" + data.cFileName;
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!remove_tree_w(child, error)) {
                    FindClose(find);
                    return false;
                }
            } else {
                SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                if (!DeleteFileW(child.c_str())) {
                    if (error) {
                        *error =
                            "删除失败: " + win::to_utf8(child) + " " + win::last_error_string();
                    }
                    FindClose(find);
                    return false;
                }
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!RemoveDirectoryW(path.c_str())) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) {
            if (error) {
                *error = win::last_error_string();
            }
            return false;
        }
    }
    return true;
}

bool copy_tree_w(const std::wstring& from, const std::wstring& to, std::string* error) {
    if (!ensure_dir_w(to)) {
        if (error) {
            *error = "无法创建目录: " + win::to_utf8(to);
        }
        return false;
    }
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((from + L"\\*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = "无法读取目录: " + win::to_utf8(from);
        }
        return false;
    }
    bool ok = true;
    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        const std::wstring source = from + L"\\" + data.cFileName;
        const std::wstring target = to + L"\\" + data.cFileName;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ok = copy_tree_w(source, target, error);
        } else {
            ok = CopyFileW(source.c_str(), target.c_str(), FALSE) != 0;
            if (!ok && error) {
                *error = "复制失败: " + win::to_utf8(source) + " " + win::last_error_string();
            }
        }
        if (!ok) {
            break;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return ok;
}

}  // namespace

std::string exe_dir() {
    static const std::string cached = win::to_utf8(win::module_dir());
    return cached;
}

std::string user_data_dir() {
    static const std::string cached = [] {
        std::wstring base = known_folder_local_appdata();
        if (base.empty()) {
            base = win::module_dir();
        }
        std::wstring dir = base + L"\\FleetingLight";
        ensure_dir_w(dir);
        return win::to_utf8(dir);
    }();
    return cached;
}

std::string user_wallpapers_dir() {
    static const std::string cached = [] {
        const std::string dir = join_path(user_data_dir(), "wallpapers");
        ensure_dir(dir);
        return dir;
    }();
    return cached;
}

std::string web_root() {
    static const std::string cached = [] {
        std::vector<wchar_t> buffer(32768);
        const DWORD size = GetEnvironmentVariableW(
            L"FL_WEB_ROOT", buffer.data(), static_cast<DWORD>(buffer.size()));
        if (size > 0 && size < buffer.size()) {
            return win::to_utf8(std::wstring(buffer.data(), size));
        }
        return join_path(exe_dir(), "web");
    }();
    return cached;
}

std::string app_version() {
    return FL_VERSION;
}

std::string join_path(std::string_view a, std::string_view b) {
    std::string out(a);
    if (!out.empty() && out.back() != '/' && out.back() != '\\') {
        out.push_back('\\');
    }
    out.append(b.data(), b.size());
    return out;
}

std::string base_name(std::string_view path) {
    const std::size_t slash = path.find_last_of("/\\");
    return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

std::string file_stem(std::string_view path) {
    std::string name      = base_name(path);
    const std::size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        name.resize(dot);
    }
    return name;
}

bool file_exists(std::string_view path) {
    const DWORD attrs = GetFileAttributesW(utf8_path(path).c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool dir_exists(std::string_view path) {
    const DWORD attrs = GetFileAttributesW(utf8_path(path).c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool ensure_dir(std::string_view path) {
    std::wstring wide = utf8_path(path);
    if (wide.empty()) {
        return false;
    }
    return ensure_dir_w(wide);
}

bool read_file(std::string_view path, std::string* out, std::string* error) {
    HANDLE handle = CreateFileW(utf8_path(path).c_str(),
                                GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = win::last_error_string();
        }
        return false;
    }
    LARGE_INTEGER size{};
    std::string data;
    if (GetFileSizeEx(handle, &size) && size.QuadPart > 0) {
        data.resize(static_cast<std::size_t>(size.QuadPart));
        DWORD read         = 0;
        std::size_t offset = 0;
        while (offset < data.size()) {
            if (!ReadFile(handle,
                          data.data() + offset,
                          static_cast<DWORD>(data.size() - offset),
                          &read,
                          nullptr)
                || read == 0) {
                break;
            }
            offset += read;
        }
        data.resize(offset);
    }
    CloseHandle(handle);
    if (out) {
        *out = std::move(data);
    }
    return true;
}

bool write_file(std::string_view path, std::string_view data, FileMode mode, std::string* error) {
    const DWORD disposition = mode == FileMode::Write ? CREATE_ALWAYS : OPEN_ALWAYS;
    HANDLE handle           = CreateFileW(utf8_path(path).c_str(),
                                          GENERIC_WRITE,
                                          FILE_SHARE_READ,
                                          nullptr,
                                          disposition,
                                          FILE_ATTRIBUTE_NORMAL,
                                          nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = win::last_error_string();
        }
        return false;
    }
    if (mode == FileMode::Append) {
        SetFilePointer(handle, 0, nullptr, FILE_END);
    }

    const char* cursor    = data.data();
    std::size_t remaining = data.size();
    if (!data.empty() && data[0] == '\xEF') {
        // UTF-8 BOM 不写进文件
        if (data.size() >= 3 && static_cast<unsigned char>(data[1]) == 0xBB
            && static_cast<unsigned char>(data[2]) == 0xBF) {
            cursor    += 3;
            remaining -= 3;
        }
    }
    while (remaining > 0) {
        DWORD written     = 0;
        const DWORD chunk = static_cast<DWORD>(remaining > 1u << 20 ? 1u << 20 : remaining);
        if (!WriteFile(handle, cursor, chunk, &written, nullptr) || written == 0) {
            if (error) {
                *error = win::last_error_string();
            }
            CloseHandle(handle);
            return false;
        }
        cursor    += written;
        remaining -= written;
    }
    CloseHandle(handle);
    return true;
}

bool list_dirs(std::string_view path, std::vector<std::string>* out, std::string* error) {
    WIN32_FIND_DATAW data{};
    const std::wstring pattern = utf8_path(path) + L"\\*";
    HANDLE find                = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = win::last_error_string();
        }
        return false;
    }
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        if (out) {
            out->push_back(win::to_utf8(data.cFileName));
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return true;
}

bool copy_file(std::string_view from, std::string_view to, std::string* error) {
    if (CopyFileW(utf8_path(from).c_str(), utf8_path(to).c_str(), FALSE) != 0) {
        return true;
    }
    if (error) {
        *error = win::last_error_string();
    }
    return false;
}

bool copy_tree(std::string_view from, std::string_view to, std::string* error) {
    return copy_tree_w(utf8_path(from), utf8_path(to), error);
}

bool remove_tree(std::string_view path, std::string* error) {
    if (!dir_exists(path)) {
        return true;
    }
    return remove_tree_w(utf8_path(path), error);
}

}  // namespace fl::env
