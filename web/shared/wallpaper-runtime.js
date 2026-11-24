// 壁纸通用运行时：画布/DPR/帧率限制/暂停/与核心通信，壁纸自己只需要实现 draw()。
//
//   import { bootWallpaper } from '../../shared/wallpaper-runtime.js';
//   bootWallpaper({
//     id: 'aurora',
//     defaults: { speed: 1, intensity: 1 },
//     create({ ctx, width, height, dpr, params }) {
//       return {
//         draw(now, dt, elapsed) { ... },
//         resize(size) { ... },
//         setParams(params) { ... },
//       };
//     },
//   });
export function bootWallpaper(definition) {
  const canvas = document.getElementById('stage');
  const ctx = canvas.getContext('2d', { alpha: false, desynchronized: true });

  const state = {
    paused: false,
    settingsOpen: false,
    fpsLimit: 60,
    params: { ...(definition.defaults || {}) },
    width: 0,
    height: 0,
    dpr: 1,
  };

  let renderer = null;
  let frameId = 0;
  let lastTime = 0;
  let startTime = 0;
  let accumulator = 0;
  let running = false;
  let readySent = false;

  // 帧率统计（调试用）
  let frames = 0;
  let statsStart = 0;

  function currentFpsLimit() {
    // 设置面板打开时，若无自定义上限就降到 30fps，省点 GPU
    if (state.settingsOpen && (!state.fpsLimit || state.fpsLimit > 30)) return 30;
    return state.fpsLimit;
  }

  function updateSize() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2.5);
    const width = Math.max(1, Math.floor(window.innerWidth));
    const height = Math.max(1, Math.floor(window.innerHeight));
    const changed = width !== state.width || height !== state.height || dpr !== state.dpr;
    if (!changed) return false;

    state.width = width;
    state.height = height;
    state.dpr = dpr;
    canvas.width = Math.floor(width * dpr);
    canvas.height = Math.floor(height * dpr);
    canvas.style.width = width + 'px';
    canvas.style.height = height + 'px';
    if (renderer && renderer.resize) {
      renderer.resize({ width, height, dpr });
    }
    return true;
  }

  function tick(now) {
    if (!running) return;
    frameId = requestAnimationFrame(tick);

    if (lastTime === 0) lastTime = now;
    const delta = now - lastTime;
    const limit = currentFpsLimit();

    if (limit > 0) {
      accumulator += delta;
      const interval = 1000 / limit;
      if (accumulator < interval) return;
      accumulator = Math.min(accumulator % interval, interval);
    }

    const dt = Math.min(delta, 100) / 1000;
    lastTime = now;
    const elapsed = (now - startTime) / 1000;

    try {
      renderer.draw(now, dt, elapsed);
    } catch (error) {
      console.error('[wallpaper] draw 异常', error);
      stop();
      report('error', String(error && error.stack ? error.stack : error));
      return;
    }

    if (!readySent) {
      readySent = true;
      if (window.bridge) window.bridge.send({ type: 'ready', id: definition.id });
    }

    if (window.__flDebug) {
      frames += 1;
      if (statsStart === 0) statsStart = now;
      if (now - statsStart >= 2000) {
        const fps = (frames * 1000) / (now - statsStart);
        frames = 0;
        statsStart = now;
        if (window.bridge) window.bridge.send({ type: 'stats', fps: Math.round(fps * 10) / 10 });
      }
    }
  }

  function start() {
    if (running) return;
    running = true;
    lastTime = 0;
    accumulator = 0;
    startTime = performance.now();
    frameId = requestAnimationFrame(tick);
  }

  function stop() {
    running = false;
    if (frameId) cancelAnimationFrame(frameId);
    frameId = 0;
  }

  function report(level, message) {
    if (window.bridge) window.bridge.send({ type: 'log', level, message });
  }

  function applyPause(paused) {
    if (state.paused === paused) return;
    state.paused = paused;
    if (paused) {
      stop();
      if (renderer && renderer.onPause) renderer.onPause();
    } else {
      if (renderer && renderer.onResume) renderer.onResume();
      start();
    }
  }

  function applyParams(params) {
    if (!params || typeof params !== 'object') return;
    state.params = { ...state.params, ...params };
    if (renderer && renderer.setParams) renderer.setParams(state.params);
  }

  function applyFps(fps) {
    const value = Number(fps);
    if (!Number.isFinite(value)) return;
    state.fpsLimit = Math.max(0, Math.min(240, Math.round(value)));
  }

  function setSettingsOpen(open) {
    const changed = state.settingsOpen !== open;
    state.settingsOpen = open;
    if (changed && renderer && renderer.setSettingsOpen) renderer.setSettingsOpen(open);
  }

  function handleMessage(message) {
    if (!message || typeof message !== 'object') return;
    switch (message.type) {
      case 'init': {
        const wallpaper = message.config && message.config.wallpaper;
        if (wallpaper) {
          applyFps(wallpaper.fps);
          applyParams(wallpaper.params);
        }
        if (message.state) {
          setSettingsOpen(!!message.state.settingsOpen);
          applyPause(!!message.state.paused);
        }
        break;
      }
      case 'config': {
        const wallpaper = message.config && message.config.wallpaper;
        if (wallpaper) {
          applyFps(wallpaper.fps);
          applyParams(wallpaper.params);
        }
        break;
      }
      case 'state': {
        setSettingsOpen(!!message.settingsOpen);
        applyPause(!!message.paused);
        break;
      }
      default:
        break;
    }
  }

  function init() {
    updateSize();

    renderer = definition.create({
      canvas,
      ctx,
      width: state.width,
      height: state.height,
      dpr: state.dpr,
      params: state.params,
      report,
    });

    window.addEventListener('resize', () => {
      if (updateSize() && renderer && renderer.resize) {
        renderer.resize({ width: state.width, height: state.height, dpr: state.dpr });
      }
    });

    if (window.bridge) window.bridge.onMessage(handleMessage);

    start();
    // 通知核心：页面已加载；核心会回 init（重复也无所谓，状态是幂等的）
    if (window.bridge) window.bridge.send({ type: 'ready', id: definition.id });
    report('info', `壁纸 ${definition.id} 已加载`);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
  } else {
    init();
  }

  return {
    state,
    get renderer() {
      return renderer;
    },
  };
}
