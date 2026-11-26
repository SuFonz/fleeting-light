// C++ 发来的提示文案翻译。
//
// 为什么要这一层：`src/core` 是平台无关的，不应该知道界面语言，所以 toast 里发的
// 一直是中文原文。设置面板拿到之后在这里查表翻一遍。
//
// 要加新提示：在 EXACT 里加一条精确匹配；带参数的（"xxx：" + 内容）加到 PREFIX。
// 没查到的原样显示，不会变成空白。

const EXACT = {
    '设置已保存': 'Settings saved',
    '已恢复默认设置': 'Defaults restored',
    '壁纸已启用': 'Wallpaper enabled',
    '壁纸已禁用，桌面背景已恢复': 'Wallpaper disabled — desktop restored',
    '已移除壁纸': 'Wallpaper removed',
    '壁纸已重启': 'Wallpaper restarted',
    '已设置开机自启': 'Autostart enabled',
    '已取消开机自启': 'Autostart disabled',
    '视频壁纸已添加': 'Video wallpaper added',
    '同名壁纸已存在，请先移除或改名':
        'A wallpaper with that name already exists — remove or rename it first',
    '这个文件夹里没有 index.html，请选网页构建产物的根目录':
        'That folder has no index.html — pick the root of a web build',
    '内置壁纸不能移除': 'Built-in wallpapers cannot be removed',
    '壁纸 id 非法': 'Invalid wallpaper id',
    '配置补丁格式错误': 'Malformed config patch',
};

// 冒号前面是固定文案，后面是具体内容，翻译前缀即可
const PREFIX = [
    ['已添加壁纸：', 'Wallpaper added: '],
    ['重置失败: ', 'Reset failed: '],
    ['保存配置失败: ', 'Could not save the config: '],
    ['复制文件失败：', 'Could not copy the file: '],
    ['复制文件夹失败：', 'Could not copy the folder: '],
    ['复制视频失败：', 'Could not copy the video: '],
    ['复制播放框架失败：', 'Could not copy the player template: '],
    ['写入播放页失败：', 'Could not write the player page: '],
    ['移除失败：', 'Could not remove: '],
    ['无法创建壁纸目录：', 'Could not create the wallpaper folder: '],
];

export function translateToast(text, language) {
    if (!text || language === 'zh') {
        return text;
    }

    if (EXACT[text]) {
        return EXACT[text];
    }

    for (const [from, to] of PREFIX) {
        if (text.startsWith(from)) {
            return to + text.slice(from.length);
        }
    }

    return text;
}
