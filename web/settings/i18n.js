// 设置面板的 i18n：字典只有一个来源 —— web/shared/i18n.json。
// 原生层（src/platform/win/ui_text.cpp）读的是同一份文件，所以加语言只需要改那个 JSON。
//
// 这里还自动建立「中文原文 → key」的反查表，用来翻译 core 层发来的 toast
// （core 不知道界面语言，一直发中文原文）。

let dictionary = {};
let currentLanguage = 'en';

export async function loadI18n() {
    const response = await fetch('../shared/i18n.json');
    if (!response.ok) {
        throw new Error(`i18n.json ${response.status}`);
    }

    const raw = await response.json();
    dictionary = {};

    for (const [key, entry] of Object.entries(raw)) {
        // 文件里还有 _comment 这类说明字段，跳过
        if (!entry || typeof entry !== 'object' || !entry.zh) {
            continue;
        }
        dictionary[key] = entry;
    }

    console.info(`[i18n] ${Object.keys(dictionary).length} keys loaded`);
}

// 'auto' 时跟随系统语言；不支持的语言一律回退英文
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
    const entry = dictionary[key];
    if (entry) {
        const text = entry[currentLanguage] || entry.en;
        if (text) {
            return text;
        }
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