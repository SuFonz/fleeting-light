// 桌面底层挂载：把壁纸窗口塞进 Explorer 的 WorkerW（图标层后面）。
#pragma once

#include "platform/win/win_util.h"

namespace fl::win {

// 让 Explorer 生成/刷新承载壁纸的 WorkerW（0x052C 私有消息）
void spawn_desktop_workerw();

// 找承载壁纸的 WorkerW：图标层（SHELLDLL_DefView）所在的顶层窗口之后的那个 WorkerW
HWND find_desktop_workerw();

// 返回虚拟屏幕矩形（多显示器时覆盖全部）
RECT virtual_desktop_rect();

// 把窗口挂到桌面底层并铺满
bool mount_to_desktop(HWND child);

}  // namespace fl::win
