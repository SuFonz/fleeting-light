// 设置面板的界面文案。
//
// 只做界面语言，壁纸内容（manifest 里的名字和描述）仍由壁纸自己决定。
// 要加一种语言：在 DICT 里加一项，再把它加进 LANGUAGES。

export const LANGUAGES = [
    { id: 'auto', label: 'System' },
    { id: 'zh', label: '中文' },
    { id: 'en', label: 'English' },
];

const DICT = {
    zh: {
        'app.title': 'Fleeting Light · 设置',

        'nav.wallpaper': '壁纸',
        'nav.params': '参数',
        'nav.settings': '设置',
        'nav.about': '关于',

        'status.connecting': '连接中…',
        'status.connected': '已连接',
        'status.preview': '未连接（预览模式）',
        'status.running': '运行中',
        'status.paused': '已暂停',

        'btn.disable': '禁用',
        'btn.enable': '启用',

        'wallpaper.title': '壁纸',
        'wallpaper.add': '＋ 添加壁纸',
        'wallpaper.importFile': '导入 HTML 文件…',
        'wallpaper.importFolder': '导入文件夹（构建产物）…',
        'wallpaper.builtin': '内置壁纸',
        'wallpaper.mine': '我的壁纸',
        'wallpaper.active': '使用中',
        'wallpaper.remove': '移除',
        'wallpaper.note.before': '导入的壁纸放在',
        'wallpaper.note.after':
            '，单文件 HTML 建议把 CSS/JS 内联，文件夹需要根目录下有 index.html。',

        'params.title': '渲染',
        'params.fps': '帧率上限',
        'params.fpsHint': '不限 = 跟随显示器刷新率；也可以自己填 1–240',
        'params.unlimited': '不限',
        'params.customPlaceholder': '自定义',
        'params.customTitle': '自定义帧率上限（1–240），回车或失焦生效',
        'params.speed': '流动速度',
        'params.intensity': '画面强度',
        'params.density': '元素密度',
        'params.densityHint': '粒子 / 光斑 / 字符列的数量',
        'params.note': '三个参数对所有壁纸生效，改动立即应用并写入 config.json。',

        'settings.title': '界面',
        'settings.language': '界面语言',
        'settings.languageHint': '默认跟随系统语言',
        'settings.power': '省电',
        'settings.pauseFullscreen': '全屏应用时暂停',
        'settings.pauseFullscreenHint': '检测到全屏游戏 / 视频时停止渲染',
        'settings.pauseLock': '锁屏时暂停',
        'settings.pauseLockHint': '息屏 / 锁屏后释放 GPU',
        'settings.pauseCovered': '窗口最大化时暂停',
        'settings.pauseCoveredHint': '有最大化窗口盖住桌面时停止渲染',
        'settings.pausePanel': '打开设置面板时暂停',
        'settings.pausePanelHint': '默认关闭，仅降低帧率',
        'settings.interaction': '互动',
        'settings.pointer': '壁纸跟随鼠标',
        'settings.pointerHint': '把光标位置推给壁纸页面，视差 / 追光 / 交互要用它',
        'settings.startup': '启动',
        'settings.autostart': '开机自启',
        'settings.autostartHint': '登录时自动启动，写入 HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run',
        'settings.devtools': '启用开发者工具',
        'settings.devtoolsHint': '下次创建视图时生效',

        'about.title': '运行信息',
        'about.fps': '实时帧率',
        'about.fpsHint': '壁纸页面每 2 秒上报一次；60 表示满帧，掉到 50 以下说明真的在丢帧',
        'about.actions': '操作',
        'about.wallpaperDevtools': '壁纸开发者工具',
        'about.wallpaperDevtoolsHint': '调试当前壁纸页面',
        'about.open': '打开',
        'about.draw': '绘制',
        'about.note':
            '壁纸页面与设置面板共用一套桥接接口：bridge.send(json) / bridge.onMessage(cb)。',

        'info.version': '版本',
        'info.platform': '平台',
        'info.config': '配置文件',
        'info.data': '数据目录',
        'info.resources': '资源目录',

        'footer.reset': '恢复默认',
        'footer.reload': '重启壁纸',
        'footer.datadir': '打开配置目录',
        'footer.quit': '退出程序',

        'pause.disabled': '已禁用，桌面背景已恢复',
        'pause.fullscreen': '全屏遮挡，暂停中',
        'pause.lock': '锁屏，暂停中',
        'pause.suspend': '系统休眠，暂停中',
        'pause.settings': '设置面板打开，暂停中',
    },

    en: {
        'app.title': 'Fleeting Light · Settings',

        'nav.wallpaper': 'Wallpapers',
        'nav.params': 'Params',
        'nav.settings': 'Settings',
        'nav.about': 'About',

        'status.connecting': 'Connecting…',
        'status.connected': 'Connected',
        'status.preview': 'Not connected (preview)',
        'status.running': 'Running',
        'status.paused': 'Paused',

        'btn.disable': 'Disable',
        'btn.enable': 'Enable',

        'wallpaper.title': 'Wallpapers',
        'wallpaper.add': '+ Add wallpaper',
        'wallpaper.importFile': 'Import HTML file…',
        'wallpaper.importFolder': 'Import folder (build output)…',
        'wallpaper.builtin': 'Built-in',
        'wallpaper.mine': 'My wallpapers',
        'wallpaper.active': 'Active',
        'wallpaper.remove': 'Remove',
        'wallpaper.note.before': 'Imported wallpapers live in',
        'wallpaper.note.after':
            '. Inline CSS/JS for single-file imports; folders need an index.html at their root.',

        'params.title': 'Rendering',
        'params.fps': 'Frame rate cap',
        'params.fpsHint': 'Unlimited = follow the display; or type any value from 1 to 240',
        'params.unlimited': 'Unlimited',
        'params.customPlaceholder': 'Custom',
        'params.customTitle': 'Custom frame cap (1–240), applied on Enter or blur',
        'params.speed': 'Speed',
        'params.intensity': 'Intensity',
        'params.density': 'Density',
        'params.densityHint': 'Particles / bokeh orbs / glyph columns',
        'params.note':
            'All three apply to every wallpaper, take effect immediately and are saved to config.json.',

        'settings.title': 'Interface',
        'settings.language': 'Language',
        'settings.languageHint': 'Defaults to your system language',
        'settings.power': 'Power saving',
        'settings.pauseFullscreen': 'Pause in fullscreen apps',
        'settings.pauseFullscreenHint': 'Stop rendering while a game or video is fullscreen',
        'settings.pauseLock': 'Pause when locked',
        'settings.pauseLockHint': 'Release the GPU when the screen is off or locked',
        'settings.pauseCovered': 'Pause when a window is maximized',
        'settings.pauseCoveredHint': 'Stop rendering while a maximized window covers the desktop',
        'settings.pausePanel': 'Pause while this panel is open',
        'settings.pausePanelHint': 'Off by default; the wallpaper only drops to 30 fps',
        'settings.interaction': 'Interaction',
        'settings.pointer': 'Stream the cursor to the wallpaper',
        'settings.pointerHint': 'Parallax, cursor glow and other effects rely on it',
        'settings.startup': 'Startup',
        'settings.autostart': 'Start with Windows',
        'settings.autostartHint': 'Written to HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run',
        'settings.devtools': 'Enable developer tools',
        'settings.devtoolsHint': 'Applies the next time a view is created',

        'about.title': 'Runtime info',
        'about.fps': 'Live frame rate',
        'about.fpsHint': 'Reported every 2 seconds; 60 is a full frame, below 50 means real drops',
        'about.actions': 'Actions',
        'about.wallpaperDevtools': 'Wallpaper DevTools',
        'about.wallpaperDevtoolsHint': 'Debug the current wallpaper page',
        'about.open': 'Open',
        'about.draw': 'draw',
        'about.note':
            'Both views share one bridge API: bridge.send(json) / bridge.onMessage(cb).',

        'info.version': 'Version',
        'info.platform': 'Platform',
        'info.config': 'Config file',
        'info.data': 'Data directory',
        'info.resources': 'Resources',

        'footer.reset': 'Reset to defaults',
        'footer.reload': 'Restart wallpaper',
        'footer.datadir': 'Open config folder',
        'footer.quit': 'Quit',

        'pause.disabled': 'Disabled — desktop background restored',
        'pause.fullscreen': 'Fullscreen app, paused',
        'pause.lock': 'Locked, paused',
        'pause.suspend': 'Suspended, paused',
        'pause.settings': 'Settings open, paused',
    },
};

let currentLanguage = 'en';

// 系统语言 → 支持的语言，不支持的一律回退到英文
function resolveLanguage(id) {
    if (id === 'zh' || id === 'en') {
        return id;
    }

    const system = (navigator.language || 'en').toLowerCase();
    return system.startsWith('zh') ? 'zh' : 'en';
}

export function setLanguage(id) {
    currentLanguage = resolveLanguage(id);
    return currentLanguage;
}

export function language() {
    return currentLanguage;
}

export function t(key, fallback) {
    const table = DICT[currentLanguage];
    if (table && table[key]) {
        return table[key];
    }

    return fallback !== undefined ? fallback : key;
}

// 把 data-i18n / data-i18n-placeholder / data-i18n-title 标到的元素翻译一遍
export function applyI18n(root = document) {
    for (const node of root.querySelectorAll('[data-i18n]')) {
        node.textContent = t(node.dataset.i18n, node.textContent);
    }

    for (const node of root.querySelectorAll('[data-i18n-placeholder]')) {
        node.placeholder = t(node.dataset.i18nPlaceholder, node.placeholder);
    }

    for (const node of root.querySelectorAll('[data-i18n-title]')) {
        node.title = t(node.dataset.i18nTitle, node.title);
    }

    document.title = t('app.title', document.title);
}
