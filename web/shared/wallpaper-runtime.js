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
  // willReadFrequently: false 让浏览器把 canvas 放在 GPU 上；alpha: false 省一次混合；
  // desynchronized: true 减少一帧延迟（对壁纸这种不需要输入的场景很合适）
  const ctx = canvas.getContext('2d', {
    alpha: false,
    desynchronized: true,
    willReadFrequently: false,
  });

  const state = {
    paused: false,
    settingsOpen: false,
    fpsLimit: 60,
    params: { ...(definition.defaults || {}) },
    width: 0,
    height: 0,
    dpr: 1,
    // 桌面互动：光标位置（归一化 0..1，相对壁纸视图），inside/down 表示在窗口内/左键按下
    pointer: { x: 0.5, y: 0.5, inside: false, down: false },
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
  // 单帧 draw() 的平均耗时（毫秒，指数平滑），调试面板/性能排查用
  let drawCost = 0;

  // ---- 分辨率预算 ----
  // 壁纸是整屏铺满的，canvas 像素数直接决定每帧的栅格化/上传成本。
  // dpr 2.5 在 4K 屏上会变成 9600x5400 = 5200 万像素，光清屏就够呛。
  // 这里做两层限制：dpr 上限 + 总像素上限（约等于 2560x1440），超了就等比降 dpr。
  const MAX_DPR = 1.5;
  const MAX_PIXELS = 2560 * 1440;

  function computeDpr(width, height) {
    let dpr = Math.min(window.devicePixelRatio || 1, MAX_DPR);
    const pixels = width * height * dpr * dpr;
    if (pixels > MAX_PIXELS) {
      dpr *= Math.sqrt(MAX_PIXELS / pixels);
    }
    return Math.max(1, dpr);
  }

  // 调试用：window.__flSetFps(60) 可以临时解除"面板打开降 30fps"的限制，方便测满帧性能
  let forcedFps = 0;
  window.__flSetFps = (value) => {
    forcedFps = Number(value) || 0;
  };

  function currentFpsLimit() {
    if (forcedFps > 0) return forcedFps;
    // 设置面板打开时，若无自定义上限就降到 30fps，省点 GPU
    if (state.settingsOpen && (!state.fpsLimit || state.fpsLimit > 30)) return 30;
    return state.fpsLimit;
  }

  function updateSize() {
    const width = Math.max(1, Math.floor(window.innerWidth));
    const height = Math.max(1, Math.floor(window.innerHeight));
    const dpr = computeDpr(width, height);
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
      // 留 15% 容差。60Hz 屏幕上 rAF 的 delta 是 16.6ms，而 1000/60 = 16.667ms，
      // 严格要求 accumulator >= interval 会让这些帧全部被丢掉，
      // 实测直接把 60fps 压到 38~40fps —— 这就是"感觉掉帧"的元凶。
      if (accumulator < interval * 0.85) return;
      accumulator = Math.max(0, Math.min(accumulator - interval, interval));
    }

    const dt = Math.min(delta, 100) / 1000;
    lastTime = now;
    const elapsed = (now - startTime) / 1000;

    try {
      const drawStart = performance.now();
      renderer.draw(now, dt, elapsed);
      drawCost = drawCost * 0.9 + (performance.now() - drawStart) * 0.1;
      window.__flDrawCost = drawCost;
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

    // 每 2 秒上报一次帧率和单帧绘制耗时：设置面板靠它显示实时性能。
    // 开销可以忽略（一次算术 + 一条消息），开着便于用户自查是否真的掉帧。
    frames += 1;
    if (statsStart === 0) statsStart = now;
    if (now - statsStart >= 2000) {
      const fps = (frames * 1000) / (now - statsStart);
      frames = 0;
      statsStart = now;
      if (window.bridge) {
        window.bridge.send({
          type: 'stats',
          fps: Math.round(fps * 10) / 10,
          drawMs: Math.round(drawCost * 100) / 100,
        });
        // 也挂到 window 上，方便用 devtools 直接看
        window.__flFps = Math.round(fps * 10) / 10;
      }
    }

    // ---- 自适应降帧：连续掉帧就降低内部分辨率 ----
    adaptQuality(delta);
  }

  // 用帧间隔判断"是否跟不上"：目标 60fps 时，16.6ms 是正常值。
  // 连续 45 帧平均超过 1.35 倍目标间隔 → 降一档（0.85x），最低到 0.6x。
  // 稳定 4 秒以上再慢慢升回去（1.06x），避免画质长期偏低。
  let slowFrames = 0;
  let fastFrames = 0;
  let quality = 1;

  function applyQuality() {
    if (!renderer || !renderer.resize) return;
    const baseDpr = computeDpr(state.width, state.height);
    const dpr = Math.max(0.6, baseDpr * quality);
    canvas.width = Math.floor(state.width * dpr);
    canvas.height = Math.floor(state.height * dpr);
    state.dpr = dpr;
    renderer.resize({ width: state.width, height: state.height, dpr });
  }

  function adaptQuality(delta) {
    const target = 1000 / (currentFpsLimit() || 60);
    if (delta > target * 1.35) {
      slowFrames += 1;
      fastFrames = 0;
    } else if (delta < target * 1.12) {
      fastFrames += 1;
      slowFrames = 0;
    }

    if (slowFrames > 45 && quality > 0.62) {
      quality = Math.max(0.6, quality * 0.85);
      slowFrames = 0;
      applyQuality();
      window.__flQuality = quality;
    } else if (fastFrames > 240 && quality < 0.999) {
      quality = Math.min(1, quality * 1.06);
      fastFrames = 0;
      applyQuality();
      window.__flQuality = quality;
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

  // 页面不可见（切到别的虚拟桌面、RDP 断开等）时彻底停下，别空转
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      stop();
    } else if (!state.paused) {
      lastTime = 0;
      start();
    }
  });

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
      case 'pointer': {
        state.pointer = {
          x: Number.isFinite(message.x) ? message.x : state.pointer.x,
          y: Number.isFinite(message.y) ? message.y : state.pointer.y,
          inside: message.inside !== false,
          down: message.down === true,
        };
        window.__flPointer = state.pointer;
        if (renderer && renderer.onPointer) renderer.onPointer(state.pointer);
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
      // 壁纸可以在 draw() 里随时读当前光标位置
      getPointer: () => state.pointer,
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
