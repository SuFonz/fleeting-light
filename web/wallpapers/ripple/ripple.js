// 涟漪：鼠标划过留下水波，按下左键炸开一个大圈。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

bootWallpaper({
  id: 'ripple',
  defaults: { speed: 1, intensity: 1, density: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    let background = null;
    let ripples = [];
    let last = { x: -1, y: -1, down: false, inside: false };
    let lightX = 0;
    let lightY = 0;

    const MAX_RIPPLES = 40;

    function buildBackground() {
      background = document.createElement('canvas');
      background.width = Math.floor(W * DPR);
      background.height = Math.floor(H * DPR);
      const b = background.getContext('2d');
      b.scale(DPR, DPR);

      const water = b.createLinearGradient(0, 0, 0, H);
      water.addColorStop(0, '#061019');
      water.addColorStop(0.5, '#04121b');
      water.addColorStop(1, '#02090f');
      b.fillStyle = water;
      b.fillRect(0, 0, W, H);

      // 极淡的网格，让水波有参照物
      b.strokeStyle = 'rgba(96, 200, 220, 0.045)';
      b.lineWidth = 1;
      const step = Math.max(48, Math.round(Math.min(W, H) / 14));
      for (let x = 0; x <= W; x += step) {
        b.beginPath();
        b.moveTo(x + 0.5, 0);
        b.lineTo(x + 0.5, H);
        b.stroke();
      }
      for (let y = 0; y <= H; y += step) {
        b.beginPath();
        b.moveTo(0, y + 0.5);
        b.lineTo(W, y + 0.5);
        b.stroke();
      }
    }

    function spawn(x, y, strength) {
      ripples.push({ x, y, r: 0, life: 0, strength });
      if (ripples.length > MAX_RIPPLES) ripples.splice(0, ripples.length - MAX_RIPPLES);
    }

    buildBackground();

    return {
      onPointer(p) {
        const px = p.x * W;
        const py = p.y * H;
        if (!p.inside) {
          last.inside = false;
          return;
        }
        if (!last.inside || last.x < 0) {
          last = { x: px, y: py, down: p.down, inside: true };
          lightX = px;
          lightY = py;
          return;
        }

        const distance = Math.hypot(px - last.x, py - last.y);
        // 移动时少发一点，不然糊成一片；按下才是重头戏
        if (distance > 70) spawn(px, py, Math.min(0.8, distance / 260));
        if (p.down && !last.down) spawn(px, py, 3.2);

        last.x = px;
        last.y = py;
        last.down = p.down;
        last.inside = true;
        lightX = px;
        lightY = py;
      },

      resize({ width: nextWidth, height: nextHeight, dpr: nextDpr }) {
        W = Math.max(1, nextWidth);
        H = Math.max(1, nextHeight);
        DPR = nextDpr;
        buildBackground();
        ripples = [];
      },

      setParams(next) {
        paramsNow = { ...paramsNow, ...next };
      },

      draw(now, dt, elapsed) {
        const speed = Number.isFinite(paramsNow.speed) ? paramsNow.speed : 1;
        const intensity = Number.isFinite(paramsNow.intensity) ? paramsNow.intensity : 1;
        const density = Number.isFinite(paramsNow.density) ? paramsNow.density : 1;

        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        ctx.drawImage(background, 0, 0, W, H);

        // 光标下的一团微光
        if (last.inside) {
          const glow = ctx.createRadialGradient(lightX, lightY, 0, lightX, lightY, Math.min(W, H) * 0.22);
          glow.addColorStop(0, `rgba(120, 235, 255, ${0.10 * intensity})`);
          glow.addColorStop(1, 'rgba(120, 235, 255, 0)');
          ctx.globalCompositeOperation = 'lighter';
          ctx.fillStyle = glow;
          ctx.fillRect(lightX - W * 0.25, lightY - W * 0.25, W * 0.5, W * 0.5);
        }

        const maxRadius = Math.min(W, H) * 0.42;
        const fade = Math.max(0.35, 1 / Math.max(0.4, density));

        ctx.globalCompositeOperation = 'lighter';
        for (let i = ripples.length - 1; i >= 0; i -= 1) {
          const ripple = ripples[i];
          ripple.life += dt * 0.75 * speed;
          ripple.r = ripple.life * maxRadius * (0.7 + ripple.strength * 0.3);
          if (ripple.life >= 1) {
            ripples.splice(i, 1);
            continue;
          }
          const alpha = (1 - ripple.life) * (1 - ripple.life) * ripple.strength * 0.5 * intensity;
          if (alpha <= 0.004) continue;

          // 外圈：柔和的波峰（宽描边 + 低透明度，读起来像水而不是线框）
          const ring = Math.max(1, (10 + ripple.strength * 10) * fade);
          ctx.beginPath();
          ctx.arc(ripple.x, ripple.y, ripple.r, 0, Math.PI * 2);
          ctx.strokeStyle = `rgba(150, 245, 255, ${alpha * 0.55})`;
          ctx.lineWidth = ring;
          ctx.stroke();

          // 内圈：一道更细更亮的波前
          const inner = ripple.r - ring * 0.9;
          if (inner > 2) {
            ctx.beginPath();
            ctx.arc(ripple.x, ripple.y, inner, 0, Math.PI * 2);
            ctx.strokeStyle = `rgba(210, 255, 255, ${alpha * 0.85})`;
            ctx.lineWidth = Math.max(1, ring * 0.28);
            ctx.stroke();
          }

          // 圈内一点点亮，模拟水面被顶起
          if (ripple.life < 0.55 && ripple.strength > 1.5) {
            const fill = ctx.createRadialGradient(
              ripple.x, ripple.y, 0, ripple.x, ripple.y, Math.max(1, ripple.r * 0.85));
            fill.addColorStop(0, `rgba(120, 220, 255, ${alpha * 0.16})`);
            fill.addColorStop(1, 'rgba(120, 220, 255, 0)');
            ctx.fillStyle = fill;
            ctx.fillRect(ripple.x - ripple.r, ripple.y - ripple.r, ripple.r * 2, ripple.r * 2);
          }
        }

        ctx.globalAlpha = 1;
        ctx.globalCompositeOperation = 'source-over';
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
