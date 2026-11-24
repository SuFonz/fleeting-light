// 数字雨：经典字符流，靠半透明黑覆盖制造拖尾。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

const GLYPHS = 'アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワン0123456789:・."=*+-<>¦|';

bootWallpaper({
  id: 'matrix',
  defaults: { speed: 1, intensity: 1, density: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    let fontSize = 16;
    let columns = 0;
    let drops = [];
    let speeds = [];
    let headFlash = [];

    function rebuild() {
      fontSize = Math.max(12, Math.round(Math.min(W, H) / 46));
      columns = Math.ceil(W / fontSize);
      drops = new Array(columns);
      speeds = new Array(columns);
      headFlash = new Array(columns);
      for (let i = 0; i < columns; i += 1) {
        drops[i] = Math.random() * (H / fontSize);
        speeds[i] = 0.55 + Math.random() * 0.95;
        headFlash[i] = 0;
      }
      ctx.fillStyle = '#040807';
      ctx.fillRect(0, 0, canvasWidth(), canvasHeight());
    }

    function canvasWidth() {
      return Math.floor(W * DPR);
    }

    function canvasHeight() {
      return Math.floor(H * DPR);
    }

    rebuild();

    return {
      resize({ width: nextWidth, height: nextHeight, dpr: nextDpr }) {
        W = Math.max(1, nextWidth);
        H = Math.max(1, nextHeight);
        DPR = nextDpr;
        rebuild();
      },
      setParams(next) {
        paramsNow = { ...paramsNow, ...next };
      },
      draw(now, dt) {
        const speed = Number.isFinite(paramsNow.speed) ? paramsNow.speed : 1;
        const density = Number.isFinite(paramsNow.density) ? paramsNow.density : 1;
        const intensity = Number.isFinite(paramsNow.intensity) ? paramsNow.intensity : 1;

        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        ctx.fillStyle = 'rgba(4, 8, 7, 0.065)';
        ctx.fillRect(0, 0, W, H);

        ctx.font = `${fontSize}px "Cascadia Mono", Consolas, "Sarasa Mono SC", monospace`;
        ctx.textBaseline = 'top';

        // 密度：小于 1 时只让一部分列下雨，大于 1 不再增加（避免同列一帧走两次）
        const active = Math.max(1, Math.round(columns * Math.min(1, density)));
        for (let i = 0; i < active; i += 1) {
          const column = i % columns;
          const x = column * fontSize;
          const y = drops[column] * fontSize;
          if (y < -fontSize) {
            drops[column] += speeds[column] * speed * dt * 14;
            continue;
          }

          const glyph = GLYPHS[(Math.random() * GLYPHS.length) | 0];
          headFlash[column] -= dt;
          if (headFlash[column] > 0) {
            ctx.fillStyle = `rgba(232, 255, 244, ${0.95 * intensity})`;
          } else {
            ctx.fillStyle = `rgba(78, 240, 150, ${0.85 * intensity})`;
          }
          ctx.fillText(glyph, x, y);

          drops[column] += speeds[column] * speed * dt * 14;
          if (y > H + fontSize * 2) {
            drops[column] = -Math.random() * 24;
            headFlash[column] = 0.35;
          }
        }

        // 顶部柔化，避免字符硬切
        const top = ctx.createLinearGradient(0, 0, 0, H * 0.08);
        top.addColorStop(0, 'rgba(4, 8, 7, 0.8)');
        top.addColorStop(1, 'rgba(4, 8, 7, 0)');
        ctx.fillStyle = top;
        ctx.fillRect(0, 0, W, H * 0.08);
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
