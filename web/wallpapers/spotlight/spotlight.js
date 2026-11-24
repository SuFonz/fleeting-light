// 追光：一盏跟着鼠标走的灯，照亮周围漂浮的尘埃。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

bootWallpaper({
  id: 'spotlight',
  defaults: { speed: 1, intensity: 1, density: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    let background = null;
    let dust = [];
    let lightX = 0;
    let lightY = 0;
    let targetX = 0;
    let targetY = 0;
    let hasPointer = false;

    function buildBackground() {
      background = document.createElement('canvas');
      background.width = Math.floor(W * DPR);
      background.height = Math.floor(H * DPR);
      const b = background.getContext('2d');
      b.scale(DPR, DPR);

      const room = b.createLinearGradient(0, 0, W * 0.6, H);
      room.addColorStop(0, '#0a0d16');
      room.addColorStop(0.55, '#080b12');
      room.addColorStop(1, '#05070c');
      b.fillStyle = room;
      b.fillRect(0, 0, W, H);

      // 一层极暗的细纹，灯光扫过时才有质感
      b.strokeStyle = 'rgba(255, 255, 255, 0.018)';
      b.lineWidth = 1;
      for (let y = 0; y < H; y += 6) {
        b.beginPath();
        b.moveTo(0, y + 0.5);
        b.lineTo(W, y + 0.5);
        b.stroke();
      }
    }

    function buildDust() {
      const count = Math.max(80, Math.round((W * H) / 26000));
      dust = Array.from({ length: Math.min(count, 700) }, () => ({
        x: Math.random() * W,
        y: Math.random() * H,
        size: 0.7 + Math.random() * 2.2,
        driftX: (Math.random() - 0.5) * 6,
        driftY: -3 - Math.random() * 9,
        phase: Math.random() * Math.PI * 2,
      }));
    }

    buildBackground();
    buildDust();
    lightX = W * 0.5;
    lightY = H * 0.5;
    targetX = lightX;
    targetY = lightY;

    return {
      onPointer(p) {
        if (!p.inside) {
          hasPointer = false;
          targetX = W * 0.5;
          targetY = H * 0.5;
          return;
        }
        hasPointer = true;
        targetX = p.x * W;
        targetY = p.y * H;
      },

      resize({ width: nextWidth, height: nextHeight, dpr: nextDpr }) {
        W = Math.max(1, nextWidth);
        H = Math.max(1, nextHeight);
        DPR = nextDpr;
        buildBackground();
        buildDust();
      },

      setParams(next) {
        paramsNow = { ...paramsNow, ...next };
      },

      draw(now, dt, elapsed) {
        const speed = Number.isFinite(paramsNow.speed) ? paramsNow.speed : 1;
        const intensity = Number.isFinite(paramsNow.intensity) ? paramsNow.intensity : 1;
        const density = Number.isFinite(paramsNow.density) ? paramsNow.density : 1;

        // 灯有惯性，跟手但不突兀
        const lerp = Math.min(1, dt * 5.5 * speed);
        lightX += (targetX - lightX) * lerp;
        lightY += (targetY - lightY) * lerp;

        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        ctx.drawImage(background, 0, 0, W, H);

        const radius = Math.min(W, H) * (0.46 + 0.05 * Math.sin(elapsed * 0.6));

        ctx.globalCompositeOperation = 'lighter';
        const light = ctx.createRadialGradient(lightX, lightY, 0, lightX, lightY, radius);
        light.addColorStop(0, `rgba(255, 244, 216, ${0.62 * intensity})`);
        light.addColorStop(0.12, `rgba(255, 226, 178, ${0.34 * intensity})`);
        light.addColorStop(0.42, `rgba(255, 196, 136, ${0.13 * intensity})`);
        light.addColorStop(0.78, `rgba(120, 150, 220, ${0.05 * intensity})`);
        light.addColorStop(1, 'rgba(90, 120, 200, 0)');
        ctx.fillStyle = light;
        ctx.fillRect(0, 0, W, H);

        // 灯的外缘：一圈很淡的冷色描边，让光有轮廓
        ctx.beginPath();
        ctx.arc(lightX, lightY, radius * 0.96, 0, Math.PI * 2);
        ctx.strokeStyle = `rgba(150, 190, 255, ${0.05 * intensity})`;
        ctx.lineWidth = 1.5;
        ctx.stroke();

        // 尘埃：离灯越近越亮
        const activeDust = Math.max(20, Math.round(dust.length * Math.min(1, density)));
        for (let i = 0; i < activeDust; i += 1) {
          const mote = dust[i];
          mote.x += mote.driftX * dt * speed;
          mote.y += mote.driftY * dt * speed;
          if (mote.y < -6) {
            mote.y = H + 6;
            mote.x = Math.random() * W;
          }
          if (mote.x < -6) mote.x = W + 6;
          if (mote.x > W + 6) mote.x = -6;

          const d = Math.hypot(mote.x - lightX, mote.y - lightY);
          const near = Math.max(0, 1 - d / radius);
          const twinkle = 0.55 + 0.45 * Math.sin(elapsed * 2.2 + mote.phase);
          const alpha = (0.10 + near * near * 1.1) * twinkle * intensity;
          if (alpha <= 0.01) continue;

          ctx.globalAlpha = Math.min(1, alpha);
          ctx.fillStyle = near > 0.3 ? 'rgba(255, 244, 222, 1)' : 'rgba(186, 210, 255, 1)';
          ctx.fillRect(mote.x, mote.y, mote.size, mote.size);
        }

        // 边缘压暗，突出光
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        const vignette = ctx.createRadialGradient(lightX, lightY, radius * 1.1, lightX, lightY, Math.max(W, H) * 1.05);
        vignette.addColorStop(0, 'rgba(0, 0, 0, 0)');
        vignette.addColorStop(1, `rgba(0, 0, 0, ${hasPointer ? 0.45 : 0.38})`);
        ctx.fillStyle = vignette;
        ctx.fillRect(0, 0, W, H);

        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
