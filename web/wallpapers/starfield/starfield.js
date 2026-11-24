// 星野：3D 推进感的星流，中心带一点星云辉光。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

bootWallpaper({
  id: 'starfield',
  defaults: { speed: 1, intensity: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    let stars = [];
    let glow = null;

    const STAR_TINTS = [
      'rgba(198, 226, 255, 1)',
      'rgba(255, 255, 255, 1)',
      'rgba(163, 197, 255, 1)',
      'rgba(255, 226, 196, 1)',
      'rgba(196, 255, 240, 1)',
    ];

    function resetStar(star, spread) {
      star.x = Math.random() * 2 - 1;
      star.y = Math.random() * 2 - 1;
      star.z = spread ? Math.random() * 0.9 + 0.1 : 1;
      star.tint = STAR_TINTS[(Math.random() * STAR_TINTS.length) | 0];
      star.size = 0.6 + Math.random() * 1.5;
      return star;
    }

    function rebuild() {
      const count = Math.max(260, Math.round((W * H) / 2600));
      stars = Array.from({ length: Math.min(count, 1600) }, () => resetStar({}, true));

      glow = ctx.createRadialGradient(W * 0.5, H * 0.5, 0, W * 0.5, H * 0.5, Math.max(W, H) * 0.7);
      glow.addColorStop(0, 'rgba(38, 66, 132, 0.10)');
      glow.addColorStop(0.45, 'rgba(22, 34, 78, 0.05)');
      glow.addColorStop(1, 'rgba(0, 0, 0, 0)');

      ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
      ctx.fillStyle = '#04060d';
      ctx.fillRect(0, 0, W, H);
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
        const intensity = Number.isFinite(paramsNow.intensity) ? paramsNow.intensity : 1;
        const density = Number.isFinite(paramsNow.density) ? paramsNow.density : 1;

        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        ctx.fillStyle = 'rgba(4, 6, 13, 0.30)';
        ctx.fillRect(0, 0, W, H);

        ctx.globalCompositeOperation = 'lighter';
        ctx.fillStyle = glow;
        ctx.fillRect(0, 0, W, H);

        const cx = W * 0.5;
        const cy = H * 0.5;
        const scale = Math.max(W, H) * 0.55;

        ctx.lineCap = 'round';
        const active = Math.max(8, Math.round(stars.length * Math.min(1, density)));
        for (let index = 0; index < active; index += 1) {
          const star = stars[index];
          const prevZ = star.z;
          star.z -= dt * speed * 0.42;
          if (star.z <= 0.02) {
            resetStar(star, false);
            continue;
          }
          const k = 1 / star.z;
          const pk = 1 / (prevZ + 0.0001);
          const x = cx + star.x * k * scale;
          const y = cy + star.y * k * scale;
          if (x < -200 || x > W + 200 || y < -200 || y > H + 200) continue;
          const px = cx + star.x * pk * scale;
          const py = cy + star.y * pk * scale;

          const depth = Math.min(1, 1.2 - star.z);
          ctx.globalAlpha = Math.min(1, depth * 0.9) * intensity;
          ctx.strokeStyle = star.tint;
          ctx.lineWidth = Math.max(0.6, star.size * depth * 1.4);
          ctx.beginPath();
          ctx.moveTo(px, py);
          ctx.lineTo(x, y);
          ctx.stroke();
        }

        ctx.globalAlpha = 1;
        ctx.globalCompositeOperation = 'source-over';
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
