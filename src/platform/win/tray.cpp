#include "platform/win/tray.h"

#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "platform/win/app_resources.h"

namespace fl::win {

HICON create_app_icon(int size) {
    if (size <= 0) {
        size = 32;
    }

    BITMAPV5HEADER header{};
    header.bV5Size        = sizeof(header);
    header.bV5Width       = size;
    header.bV5Height      = -size;  // 负值表示自上而下
    header.bV5Planes      = 1;
    header.bV5BitCount    = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask     = 0x00FF0000;
    header.bV5GreenMask   = 0x0000FF00;
    header.bV5BlueMask    = 0x000000FF;
    header.bV5AlphaMask   = 0xFF000000;

    void* bits    = nullptr;
    HDC screen    = GetDC(nullptr);
    HDC memory    = CreateCompatibleDC(screen);
    HBITMAP color = CreateDIBSection(
        screen, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color || !bits) {
        if (memory) {
            DeleteDC(memory);
        }
        return nullptr;
    }

    auto* pixels     = static_cast<std::uint32_t*>(bits);
    const float half = static_cast<float>(size) / 2.0f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx       = (static_cast<float>(x) + 0.5f - half) / half;
            const float dy       = (static_cast<float>(y) + 0.5f - half) / half;
            const float distance = std::sqrt(dx * dx + dy * dy);
            float alpha          = std::clamp(1.0f - (distance - 0.55f) / 0.35f, 0.0f, 1.0f);
            alpha                = alpha * alpha * (3.0f - 2.0f * alpha);  // smoothstep

            // 斜向渐变：青 -> 紫
            const float t  = std::clamp((dx + dy + 2.0f) / 4.0f, 0.0f, 1.0f);
            const auto mix = [t](float a, float b) { return a + (b - a) * t; };
            const auto r   = static_cast<std::uint32_t>(mix(0x31, 0x8B) * 255.0f) & 0xFF;
            const auto g   = static_cast<std::uint32_t>(mix(0xE1, 0x5C) * 255.0f) & 0xFF;
            const auto b   = static_cast<std::uint32_t>(mix(0xC8, 0xF6) * 255.0f) & 0xFF;
            const auto a   = static_cast<std::uint32_t>(alpha * 255.0f) & 0xFF;
            pixels[y * size + x] =
                (a << 24) | (r << 16) | (g << 8) | b;  // BGRA in memory -> 0xAARRGGBB 布局
        }
    }

    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO info{};
    info.fIcon    = TRUE;
    info.hbmMask  = mask;
    info.hbmColor = color;
    HICON icon    = CreateIconIndirect(&info);

    DeleteObject(color);
    if (mask) {
        DeleteObject(mask);
    }
    DeleteDC(memory);
    return icon;
}

TrayIcon::~TrayIcon() {
    remove();
}

bool TrayIcon::add(HWND owner, UINT callback_message, std::wstring_view tooltip) {
    if (added_) {
        return true;
    }
    // 优先用 exe 里嵌的图标（和任务栏一致），加载不到再退回运行时画的渐变圆
    icon_       = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
                                                MAKEINTRESOURCEW(FL_IDI_APP),
                                                IMAGE_ICON,
                                                GetSystemMetrics(SM_CXSMICON),
                                                GetSystemMetrics(SM_CYSMICON),
                                                LR_DEFAULTCOLOR));
    icon_owned_ = icon_ != nullptr;
    if (!icon_) {
        icon_       = create_app_icon(32);
        icon_owned_ = icon_ != nullptr;
    }

    data_                  = NOTIFYICONDATAW{};
    data_.cbSize           = sizeof(data_);
    data_.hWnd             = owner;
    data_.uID              = 1;
    data_.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callback_message;
    data_.hIcon            = icon_ ? icon_ : LoadIconW(nullptr, IDI_APPLICATION);
    const std::wstring text(tooltip);
    wcsncpy(data_.szTip, text.c_str(), std::size(data_.szTip) - 1);

    added_ = Shell_NotifyIconW(NIM_ADD, &data_) == TRUE;
    if (added_) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
    return added_;
}

void TrayIcon::remove() {
    if (!added_) {
        if (icon_ && icon_owned_) {
            DestroyIcon(icon_);
        }
        icon_       = nullptr;
        icon_owned_ = false;
        return;
    }
    Shell_NotifyIconW(NIM_DELETE, &data_);
    added_ = false;
    if (icon_ && icon_owned_) {
        DestroyIcon(icon_);
    }
    icon_       = nullptr;
    icon_owned_ = false;
}

bool TrayIcon::update_tooltip(std::wstring_view tooltip) {
    if (!added_) {
        return false;
    }
    data_.uFlags = NIF_TIP | NIF_SHOWTIP;
    const std::wstring text(tooltip);
    wcsncpy(data_.szTip, text.c_str(), std::size(data_.szTip) - 1);
    data_.szTip[std::size(data_.szTip) - 1] = L'\0';
    return Shell_NotifyIconW(NIM_MODIFY, &data_) == TRUE;
}

}  // namespace fl::win
