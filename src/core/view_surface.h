// 统一视图抽象接口：上层只认 send/onMessage，屏蔽底层是 WebView2 还是 WebKitGTK / QtWebEngine。
#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace fl::core {

enum class SurfaceKind { Wallpaper, Settings };

class IViewSurface {
public:
    using MessageHandler = std::function<void(std::string_view json)>;

    virtual ~IViewSurface() = default;

    // relative_url 形如 "wallpapers/matrix/index.html"，由平台拼成最终 URL
    virtual void create(const std::string& relative_url,
                        MessageHandler on_message,
                        std::function<void()> on_ready)    = 0;
    virtual void destroy()                                 = 0;
    // 换页面（切换壁纸）或重新加载（内部带缓存击穿参数）
    virtual void navigate(const std::string& relative_url) = 0;
    virtual void reload()                                  = 0;
    virtual void set_visible(bool visible)                 = 0;
    virtual void send(std::string_view json)               = 0;
    virtual bool ready() const                             = 0;
    // 平台特有能力的兜底入口（Windows: 打开壁纸视图的 DevTools）
    virtual void open_devtools() {
    }
};

}  // namespace fl::core
