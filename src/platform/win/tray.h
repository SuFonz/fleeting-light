// 托盘图标：程序没有主窗口，开关/暂停/退出都靠托盘菜单。
#pragma once

#include <string>
#include <string_view>

#include "platform/win/win_util.h"

#include <shellapi.h>  // 必须在 windows.h 之后

namespace fl::win {

class TrayIcon {
 public:
  TrayIcon() = default;
  ~TrayIcon();

  TrayIcon(const TrayIcon&) = delete;
  TrayIcon& operator=(const TrayIcon&) = delete;

  bool add(HWND owner, UINT callback_message, std::wstring_view tooltip);
  void remove();
  bool update_tooltip(std::wstring_view tooltip);
  bool visible() const { return added_; }

 private:
  NOTIFYICONDATAW data_{};
  bool added_ = false;
  HICON icon_ = nullptr;
  bool icon_owned_ = false;  // LoadImage 拿到的图标也要自己销毁
};

// 兜底：加载不到 exe 资源时，运行时画一个渐变圆当托盘图标
HICON create_app_icon(int size);

}  // namespace fl::win
