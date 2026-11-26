// 设置面板：只做三件事——渲染状态、把用户操作发出去、把主进程回来的状态画上去。
const bridge = window.bridge;

const PAUSE_REASON_TEXT = {
  disabled: '已禁用，桌面背景已恢复',
  fullscreen: '全屏遮挡，暂停中',
  lock: '锁屏，暂停中',
  suspend: '系统休眠，暂停中',
  settings: '设置面板打开，暂停中',
};

const view = {
  config: null,
  info: null,
  status: null,
};

let pendingPatch = null;
let patchTimer = 0;

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => Array.from(document.querySelectorAll(selector));

// ------------------------------------------------------------------ 通信

function send(message) {
  if (bridge) {
    bridge.send(message);
  }
}

function queuePatch(patch) {
  pendingPatch = mergeDeep(pendingPatch || {}, patch);
  if (patchTimer) {
    clearTimeout(patchTimer);
  }
  patchTimer = setTimeout(flushPatch, 140);
}

function flushPatch() {
  if (!pendingPatch) {
    return;
  }
  const config = pendingPatch;
  pendingPatch = null;
  patchTimer = 0;
  send({ type: 'update', config });
}

function mergeDeep(target, patch) {
  const output = { ...target };
  for (const [key, value] of Object.entries(patch)) {
    if (value && typeof value === 'object' && !Array.isArray(value)) {
      output[key] = mergeDeep(target[key] || {}, value);
    } else {
      output[key] = value;
    }
  }
  return output;
}

// ------------------------------------------------------------------ 渲染

// 没有预览图（或图加载失败）时的占位图标：一个简单的风景图符号
function placeholderIcon() {
  const NS = 'http://www.w3.org/2000/svg';
  const svg = document.createElementNS(NS, 'svg');
  svg.setAttribute('viewBox', '0 0 24 24');
  svg.setAttribute('class', 'wp-placeholder');
  svg.setAttribute('aria-hidden', 'true');

  const frame = document.createElementNS(NS, 'rect');
  frame.setAttribute('x', '3');
  frame.setAttribute('y', '5');
  frame.setAttribute('width', '18');
  frame.setAttribute('height', '14');
  frame.setAttribute('rx', '2');

  const sun = document.createElementNS(NS, 'circle');
  sun.setAttribute('cx', '8.5');
  sun.setAttribute('cy', '9.5');
  sun.setAttribute('r', '1.6');

  const hill = document.createElementNS(NS, 'path');
  hill.setAttribute('d', 'M4 17l4.5-5 3 3.2L15 11l5 6z');

  svg.append(frame, sun, hill);
  return svg;
}

// 壁纸预览图：内置的走 fleeting.light，导入的走 fleeting.local。
// 没有预览图时返回空串，由调用方显示占位图标。
function previewUrl(item) {
  if (!item.preview) {
    return '';
  }
  const base = item.user ? 'https://fleeting.local/' : 'https://fleeting.light/';
  return base + item.preview;
}

function createWallpaperRow(item, activeId) {
  const row = document.createElement('button');
  row.type = 'button';
  row.className = 'wp-row' + (item.id === activeId ? ' active' : '');
  row.dataset.id = item.id;

  const thumb = document.createElement('span');
  thumb.className = 'wp-swatch';
  const url = previewUrl(item);
  if (url) {
    const img = document.createElement('img');
    img.src = url;
    img.alt = '';
    img.loading = 'lazy';
    // 图挂了就退回占位图标，别留一块破图
    img.addEventListener('error', () => {
      thumb.replaceChildren(placeholderIcon());
    });
    thumb.append(img);
  } else {
    thumb.append(placeholderIcon());
  }

  const meta = document.createElement('span');
  meta.className = 'wp-meta';
  const name = document.createElement('span');
  name.className = 'wp-name';
  name.textContent = item.name || item.id;
  const desc = document.createElement('span');
  desc.className = 'wp-desc';
  desc.textContent = item.description || '';
  meta.append(name, desc);

  row.append(thumb, meta);

  if (item.user) {
    const remove = document.createElement('span');
    remove.className = 'wp-remove';
    remove.textContent = '移除';
    remove.addEventListener('click', (event) => {
      event.stopPropagation();
      const label = item.name || item.id;
      if (!window.confirm(`确定移除「${label}」吗？\n只会删掉导入的那份副本，不会动你原来的文件夹。`)) {
        return;
      }
      send({ type: 'action', name: 'remove_wallpaper', id: item.id });
    });
    row.append(remove);
  }

  const check = document.createElement('span');
  check.className = 'wp-check';
  check.textContent = '✓';
  row.append(check);

  row.addEventListener('click', () => {
    if (item.id === activeId) {
      return;
    }
    // 切壁纸要立刻生效，不走防抖
    flushPatch();
    send({ type: 'update', config: { wallpaper: { id: item.id } } });
  });

  return row;
}

function renderWallpapers() {
  const info = view.info;
  if (!info || !Array.isArray(info.wallpapers)) {
    return;
  }

  const activeId = view.config && view.config.wallpaper ? view.config.wallpaper.id : null;
  const builtin = info.wallpapers.filter((item) => !item.user);
  const user = info.wallpapers.filter((item) => item.user);

  const builtinList = $('#builtinList');
  if (builtinList) {
    builtinList.innerHTML = '';
    for (const item of builtin) {
      builtinList.append(createWallpaperRow(item, activeId));
    }
  }

  const userList = $('#userList');
  if (userList) {
    userList.innerHTML = '';
    for (const item of user) {
      userList.append(createWallpaperRow(item, activeId));
    }
  }

  const userGroup = $('#userGroup');
  if (userGroup) {
    userGroup.hidden = user.length === 0;
  }

  const hint = $('#userDirHint');
  if (hint) {
    hint.textContent = info.userWallpapersDir || '…';
  }
}

// 帧率预设：这几个值由按钮负责，其余值走自定义输入框
const FPS_PRESETS = [0, 30, 60, 120];
const FPS_MIN = 1;
const FPS_MAX = 240;

function renderFps(wallpaper) {
  const fps = Number.isFinite(wallpaper.fps) ? wallpaper.fps : 60;

  for (const button of $$('#fpsGroup button')) {
    button.classList.toggle('active', Number(button.dataset.fps) === fps);
  }

  const custom = $('#fpsCustom');
  if (!custom) {
    return;
  }

  const isPreset = FPS_PRESETS.indexOf(fps) >= 0;
  // 正在输入时不要覆盖用户手上的值
  if (document.activeElement !== custom) {
    custom.value = isPreset ? '' : String(fps);
  }
  custom.classList.toggle('filled', !isPreset);
}

function renderParams() {
  const wallpaper = (view.config && view.config.wallpaper) || {};
  renderFps(wallpaper);

  const params = wallpaper.params || {};
  const speed = Number.isFinite(params.speed) ? params.speed : 1;
  const intensity = Number.isFinite(params.intensity) ? params.intensity : 1;
  const density = Number.isFinite(params.density) ? params.density : 1;

  const speedRange = $('#speedRange');
  const intensityRange = $('#intensityRange');
  const densityRange = $('#densityRange');
  if (document.activeElement !== speedRange) {
    speedRange.value = String(speed);
  }
  if (document.activeElement !== intensityRange) {
    intensityRange.value = String(intensity);
  }
  if (document.activeElement !== densityRange) {
    densityRange.value = String(density);
  }
  $('#speedValue').textContent = speed.toFixed(1) + '×';
  $('#intensityValue').textContent = intensity.toFixed(1);
  $('#densityValue').textContent = density.toFixed(1);
}

function setSwitch(element, value) {
  element.setAttribute('aria-checked', value ? 'true' : 'false');
}

function renderSwitches() {
  const app = (view.config && view.config.app) || {};
  setSwitch($('#pauseOnFullscreen'), app.pauseOnFullscreen !== false);
  setSwitch($('#pauseOnLock'), app.pauseOnLock !== false);
  setSwitch($('#pauseWhenCovered'), app.pauseWhenCovered !== false);
  setSwitch($('#pauseWhenSettingsOpen'), app.pauseWhenSettingsOpen === true);
  setSwitch($('#startWithWindows'), app.startWithWindows === true);
  setSwitch($('#devTools'), app.devTools === true);
  setSwitch($('#pointerTracking'), app.pointerTracking !== false);
}

function renderStatus() {
  const pill = $('#statusPill');
  const text = $('#statusText');
  const button = $('#pauseButton');
  const status = view.status;

  pill.classList.remove('paused', 'offline');

  if (!status) {
    pill.classList.add('offline');
    text.textContent = bridge && bridge.connected ? '已连接' : '未连接（预览模式）';
    button.textContent = '禁用';
    return;
  }

  if (status.paused) {
    pill.classList.add('paused');
    text.textContent = PAUSE_REASON_TEXT[status.reason] || '已暂停';
  } else {
    text.textContent = '运行中';
  }
  button.textContent = status.enabled === false ? '启用' : '禁用';
}

function renderInfo() {
  const info = view.info;
  if (!info) {
    return;
  }
  $('#versionLabel').textContent = 'v' + (info.version || '—');

  const rows = [
    ['版本', info.version || '—'],
    ['平台', info.platform || '—'],
    ['WebView2', info.webview || '—'],
    ['配置文件', info.configPath || '—'],
    ['数据目录', info.dataDir || '—'],
    ['资源目录', info.webRoot || '—'],
  ];

  const list = $('#infoList');
  list.innerHTML = '';
  for (const [key, value] of rows) {
    const dt = document.createElement('dt');
    dt.textContent = key;
    const dd = document.createElement('dd');
    dd.textContent = value;
    list.append(dt, dd);
  }
}

function toast(message, level) {
  const element = $('#toast');
  element.textContent = message;
  element.className = 'toast show ' + (level || 'info');
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => {
    element.className = 'toast';
  }, 2200);
}

// ------------------------------------------------------------------ 事件

function bindEvents() {
  for (const item of $$('.nav-item')) {
    item.addEventListener('click', () => {
      for (const other of $$('.nav-item')) {
        other.classList.toggle('active', other === item);
      }
      for (const pane of $$('.pane')) {
        pane.classList.toggle('active', pane.id === 'pane-' + item.dataset.pane);
      }
    });
  }

  // 「添加壁纸」下拉
  const addButton = $('#addWallpaperButton');
  const addMenu = $('#addWallpaperMenu');
  if (addButton && addMenu) {
    addButton.addEventListener('click', (event) => {
      event.stopPropagation();
      addMenu.hidden = !addMenu.hidden;
    });
    for (const item of $$('#addWallpaperMenu button')) {
      item.addEventListener('click', (event) => {
        event.stopPropagation();
        addMenu.hidden = true;
        send({ type: 'action', name: 'import_wallpaper', kind: item.dataset.kind });
      });
    }
    document.addEventListener('click', () => {
      addMenu.hidden = true;
    });
  }

  for (const button of $$('#fpsGroup button')) {
    button.addEventListener('click', () => {
      queuePatch({ wallpaper: { fps: Number(button.dataset.fps) } });
      for (const other of $$('#fpsGroup button')) {
        other.classList.toggle('active', other === button);
      }
      // 选了预设就把自定义输入清掉
      const custom = $('#fpsCustom');
      if (custom) {
        custom.value = '';
        custom.classList.remove('filled');
      }
    });
  }

  const fpsCustom = $('#fpsCustom');
  if (fpsCustom) {
    fpsCustom.addEventListener('change', () => {
      const raw = fpsCustom.value.trim();
      if (raw === '') {
        return;
      }

      let value = Math.round(Number(raw));
      if (!Number.isFinite(value)) {
        fpsCustom.value = '';
        return;
      }

      value = Math.max(FPS_MIN, Math.min(FPS_MAX, value));
      fpsCustom.value = String(value);
      fpsCustom.classList.add('filled');

      // 自定义值生效时，预设按钮全部取消选中
      for (const other of $$('#fpsGroup button')) {
        other.classList.remove('active');
      }
      queuePatch({ wallpaper: { fps: value } });
    });

    // Enter 直接提交，不用先点别处
    fpsCustom.addEventListener('keydown', (event) => {
      if (event.key === 'Enter') {
        event.preventDefault();
        fpsCustom.blur();
      }
    });
  }

  $('#speedRange').addEventListener('input', (event) => {
    const value = Number(event.target.value);
    $('#speedValue').textContent = value.toFixed(1) + '×';
    queuePatch({ wallpaper: { params: { speed: value } } });
  });

  $('#intensityRange').addEventListener('input', (event) => {
    const value = Number(event.target.value);
    $('#intensityValue').textContent = value.toFixed(1);
    queuePatch({ wallpaper: { params: { intensity: value } } });
  });

  $('#densityRange').addEventListener('input', (event) => {
    const value = Number(event.target.value);
    $('#densityValue').textContent = value.toFixed(1);
    queuePatch({ wallpaper: { params: { density: value } } });
  });

  const switchBindings = [
    ['#pauseOnFullscreen', 'app', 'pauseOnFullscreen'],
    ['#pauseOnLock', 'app', 'pauseOnLock'],
    ['#pauseWhenCovered', 'app', 'pauseWhenCovered'],
    ['#pauseWhenSettingsOpen', 'app', 'pauseWhenSettingsOpen'],
    ['#startWithWindows', 'app', 'startWithWindows'],
    ['#devTools', 'app', 'devTools'],
    ['#pointerTracking', 'app', 'pointerTracking'],
  ];

  for (const [selector, group, key] of switchBindings) {
    const element = $(selector);
    element.addEventListener('click', () => {
      const next = element.getAttribute('aria-checked') !== 'true';
      setSwitch(element, next);
      queuePatch({ [group]: { [key]: next } });
    });
  }

  $('#pauseButton').addEventListener('click', () => {
    // 显示中就禁用，禁用中就启用
    const currentlyEnabled = !(view.status && view.status.enabled === false);
    send({ type: 'set_enabled', enabled: !currentlyEnabled });
  });

  $('#resetButton').addEventListener('click', () => send({ type: 'action', name: 'reset_config' }));
  $('#reloadButton').addEventListener('click', () => send({ type: 'action', name: 'restart_wallpaper' }));
  $('#dataDirButton').addEventListener('click', () => send({ type: 'action', name: 'open_data_dir' }));
  $('#devtoolsButton').addEventListener('click', () =>
    send({ type: 'action', name: 'open_wallpaper_devtools' }));
  $('#quitButton').addEventListener('click', () => send({ type: 'action', name: 'quit' }));

  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') {
      flushPatch();
      send({ type: 'action', name: 'close' });
    }
  });
}

// ------------------------------------------------------------------ 启动

function handleMessage(message) {
  if (!message || typeof message !== 'object') {
    return;
  }

  switch (message.type) {
    case 'init':
      view.config = message.config || view.config;
      view.info = message.info || view.info;
      view.status = message.state || view.status;
      renderWallpapers();
      renderParams();
      renderSwitches();
      renderInfo();
      renderStatus();
      break;

    case 'config':
      view.config = message.config || view.config;
      renderWallpapers();
      renderParams();
      renderSwitches();
      break;

    case 'state':
      view.status = message;
      renderStatus();
      break;

    case 'stats': {
      const node = document.getElementById('fpsValue');
      if (node) {
        const fps = Number(message.fps) || 0;
        const drawMs = Number(message.drawMs) || 0;
        node.textContent = `${fps.toFixed(1)} fps · 绘制 ${drawMs.toFixed(2)} ms`;
        // 低于 50 帧标红，一眼看出是否真掉帧
        node.classList.toggle('warn', fps > 0 && fps < 50);
      }
      break;
    }

    case 'toast':
      toast(message.message || '', message.level);
      break;

    default:
      break;
  }
}

function init() {
  bindEvents();
  renderStatus();
  if (bridge) {
    bridge.onMessage(handleMessage);
  }
  // 页面加载完成 → 请求一次完整状态
  send({ type: 'ready' });
  setTimeout(() => send({ type: 'request_init' }), 350);
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init, { once: true });
} else {
  init();
}
