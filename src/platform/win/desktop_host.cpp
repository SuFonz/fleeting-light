#include "platform/win/desktop_host.h"

#include <cstdint>
#include <string>

#include "core/log.h"
#include "platform/win/win_util.h"

namespace fl::win {
namespace {

constexpr UINT kSpawnWorkerMessage = 0x052C;

// 真正能当壁纸宿主的大窗口：可见且面积够大（排除 Explorer 内部 136x38 之类的小 WorkerW）
bool is_usable_host(HWND hwnd) {
    if (!hwnd || !IsWindowVisible(hwnd)) {
        return false;
    }
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return false;
    }
    return (rect.right - rect.left) > 200 && (rect.bottom - rect.top) > 200;
}

// 经典布局：图标宿主所在顶层窗口之后（z 序更靠下）的那个 WorkerW 才是壁纸层
HWND find_classic_workerw() {
    struct Search {
        HWND defview_host = nullptr;
    } search;

    EnumWindows(
        [](HWND top, LPARAM param) -> BOOL {
            if (!FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr)) {
                return TRUE;
            }
            reinterpret_cast<Search*>(param)->defview_host = top;
            return FALSE;
        },
        reinterpret_cast<LPARAM>(&search));

    if (!search.defview_host) {
        return nullptr;
    }

    HWND worker = FindWindowExW(nullptr, search.defview_host, L"WorkerW", nullptr);
    while (worker) {
        if (is_usable_host(worker)) {
            return worker;
        }
        worker = FindWindowExW(nullptr, worker, L"WorkerW", nullptr);
    }
    return nullptr;
}

}  // namespace

void spawn_desktop_workerw() {
    HWND progman = FindWindowW(L"Progman", nullptr);
    if (!progman) {
        log_warn("未找到 Progman，桌面挂载可能失败");
        return;
    }
    DWORD_PTR result = 0;
    SendMessageTimeoutW(
        progman, kSpawnWorkerMessage, 0xD, 0, SMTO_NORMAL | SMTO_ABORTIFHUNG, 1000, &result);
    // 部分版本需要第二发（lParam=1）才会生成壁纸层
    SendMessageTimeoutW(
        progman, kSpawnWorkerMessage, 0xD, 1, SMTO_NORMAL | SMTO_ABORTIFHUNG, 1000, &result);
}

HWND find_desktop_workerw() {
    HWND progman = FindWindowW(L"Progman", nullptr);

    // 新布局（Win11 及部分 Win10）：Progman 内部会生成一个铺满屏幕的 WorkerW 子窗口，
    // 它在 z 序上位于 SHELLDLL_DefView（图标层）之后，正好是壁纸层。
    if (progman) {
        HWND child = FindWindowExW(progman, nullptr, L"WorkerW", nullptr);
        while (child) {
            if (is_usable_host(child)) {
                return child;
            }
            child = FindWindowExW(progman, child, L"WorkerW", nullptr);
        }
    }

    // 经典布局：顶层 WorkerW
    if (HWND worker = find_classic_workerw()) {
        return worker;
    }

    // 兜底：直接挂 Progman（壁纸会被图标层盖住，但总比没有强）
    return progman;
}

RECT virtual_desktop_rect() {
    RECT rect{};
    rect.left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rect.top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rect.right  = rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rect.bottom = rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        rect = RECT{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    }
    return rect;
}

bool mount_to_desktop(HWND child) {
    if (!child) {
        return false;
    }
    HWND host = find_desktop_workerw();
    if (!host) {
        log_error("找不到可用的 WorkerW / Progman");
        return false;
    }

    SetParent(child, host);
    const LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
    SetWindowLongPtrW(
        child, GWL_STYLE, (style & ~static_cast<LONG_PTR>(WS_POPUP)) | WS_CHILD | WS_VISIBLE);

    RECT client{};
    GetClientRect(host, &client);
    int width  = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) {
        const RECT desktop = virtual_desktop_rect();
        width              = desktop.right - desktop.left;
        height             = desktop.bottom - desktop.top;
    }

    SetWindowPos(child,
                 HWND_BOTTOM,
                 0,
                 0,
                 width,
                 height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    log_info("壁纸已挂到桌面底层: host=" + std::to_string(reinterpret_cast<std::uintptr_t>(host))
             + " class=" + to_utf8([host] {
                   wchar_t name[64] = {};
                   GetClassNameW(host, name, 64);
                   return std::wstring(name);
               }())
             + " " + std::to_string(width) + "x" + std::to_string(height));
    return IsWindowVisible(child) != FALSE;
}

}  // namespace fl::win
