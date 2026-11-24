// 极光壁纸：预渲染的星空背景 + 多条流动光幕 + 浮尘 + 暗角。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

bootWallpaper({
  id: 'aurora',
  defaults: { speed: 1, intensity: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    let background = null;
    let vignette = null;
    let bands = [];
    let dust = [];

    const BAND_COLORS = [
      [90, 255, 214],   // 青绿
      [70, 200, 255],   // 天蓝
      [150, 130, 255],  // 紫
      [86, 240, 160],   // 薄荷
    ];

    function makeBackground() {
      background = document.createElement('canvas');
      background.width = Math.floor(W * DPR);
      background.height = Math.floor(H * DPR);
      const b = background.getContext('2d');
      b.scale(DPR, DPR);

      const sky = b.createLinearGradient(0, 0, 0, H);
      sky.addColorStop(0, '#04060d');
      sky.addColorStop(0.5, '#060a16');
      sky.addColorStop(1, '#080d1d');
      b.fillStyle = sky;
      b.fillRect(0, 0, W, H);

      // 星云：几团极淡的彩色光斑
      const clouds = [
        { x: 0.2, y: 0.75, r: 0.5, color: 'rgba(40, 90, 200, 0.10)' },
        { x: 0.8, y: 0.85, r: 0.45, color: 'rgba(120, 60, 200, 0.08)' },
        { x: 0.5, y: 0.3, r: 0.6, color: 'rgba(20, 90, 120, 0.06)' },
      ];
      for (const cloud of clouds) {
        const radius = Math.max(W, H) * cloud.r;
        const gradient = b.createRadialGradient(cloud.x * W, cloud.y * H, 0, cloud.x * W, cloud.y * H, radius);
        gradient.addColorStop(0, cloud.color);
        gradient.addColorStop(1, 'rgba(0, 0, 0, 0)');
        b.fillStyle = gradient;
        b.fillRect(0, 0, W, H);
      }

      // 星点
      const starCount = Math.round((W * H) / 5200);
      for (let i = 0; i < starCount; i += 1) {
        const x = Math.random() * W;
        const y = Math.random() * H * 0.92;
        const size = Math.random() < 0.86 ? 1 : 1.6;
        const alpha = 0.16 + Math.random() * 0.55;
        b.fillStyle = `rgba(${200 + Math.random() * 55 | 0}, ${215 + Math.random() * 40 | 0}, 255, ${alpha})`;
        b.fillRect(x, y, size, size);
      }
    }

    function makeVignette() {
      vignette = document.createElement('canvas');
      vignette.width = Math.floor(W * DPR);
      vignette.height = Math.floor(H * DPR);
      const v = vignette.getContext('2d');
      v.scale(DPR, DPR);
      const gradient = v.createRadialGradient(W * 0.5, H * 0.55, Math.min(W, H) * 0.18, W * 0.5, H * 0.55,
        Math.max(W, H) * 0.78);
      gradient.addColorStop(0, 'rgba(0, 0, 0, 0)');
      gradient.addColorStop(1, 'rgba(0, 0, 0, 0.55)');
      v.fillStyle = gradient;
      v.fillRect(0, 0, W, H);
    }

    function makeBands() {
      bands = BAND_COLORS.map((color, index) => {
        const base = H * (0.34 + index * 0.11);
        const amp = H * (0.05 + 0.03 * index);
        const thickness = H * (0.30 + 0.06 * index);
        const top = base - amp;
        const bottom = base + thickness;
        const gradient = ctx.createLinearGradient(0, top, 0, bottom);
        const [r, g, b] = color;
        // 多段停靠点，避免大块渐变出现色阶断层
        const stops = [0, 0.14, 0.3, 0.44, 0.56, 0.7, 0.86, 1];
        for (const stop of stops) {
          const envelope = Math.sin(Math.PI * Math.min(1, Math.max(0, stop)));
          const alpha = (0.02 + 0.62 * envelope).toFixed(3);
          gradient.addColorStop(stop, `rgba(${r}, ${g}, ${b}, ${alpha})`);
        }
        return {
          color,
          gradient,
          base,
          amp,
          thickness,
          speed: 0.16 + index * 0.055,
          phase: index * 2.2,
          freq: 1.5 + index * 0.55,
          alpha: 0.5 - index * 0.06,
        };
      });
    }

    function makeDust() {
      const count = Math.round((W * H) / 42000);
      dust = Array.from({ length: Math.max(24, count) }, () => ({
        x: Math.random() * W,
        y: Math.random() * H,
        size: 0.7 + Math.random() * 1.7,
        speed: 4 + Math.random() * 14,
        drift: (Math.random() - 0.5) * 10,
        alpha: 0.08 + Math.random() * 0.3,
        phase: Math.random() * Math.PI * 2,
      }));
    }

    function rebuild() {
      makeBackground();
      makeVignette();
      makeBands();
      makeDust();
    }

    function edgeY(band, x, time) {
      const t = x / W;
      return band.base +
        Math.sin(t * band.freq * Math.PI * 2 + band.phase + time * band.speed) * band.amp +
        Math.sin(t * band.freq * 4.1 + band.phase * 0.7 + time * band.speed * 0.6) * band.amp * 0.35;
    }

    function drawBand(band, time, intensity) {
      const step = Math.max(14, W / 80);

      ctx.beginPath();
      for (let x = 0; x <= W + step; x += step) {
        const y = edgeY(band, Math.min(x, W), time);
        if (x === 0) ctx.moveTo(0, y);
        else ctx.lineTo(Math.min(x, W), y);
      }
      for (let x = W + step; x >= 0; x -= step) {
        const px = Math.min(x, W);
        const depth = band.thickness * (0.85 + 0.15 * Math.sin(px * 0.004 + time * 0.3));
        ctx.lineTo(px, edgeY(band, px, time) + depth);
      }
      ctx.closePath();
      ctx.globalAlpha = band.alpha * intensity;
      ctx.fillStyle = band.gradient;
      ctx.fill();
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

      draw(now, dt, elapsed) {
        const speed = Number.isFinite(paramsNow.speed) ? paramsNow.speed : 1;
        const intensity = Number.isFinite(paramsNow.intensity) ? paramsNow.intensity : 1;
        const density = Number.isFinite(paramsNow.density) ? paramsNow.density : 1;
        const time = elapsed * speed;

        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        ctx.drawImage(background, 0, 0, W, H);

        ctx.globalCompositeOperation = 'lighter';
        for (const band of bands) drawBand(band, time, intensity);

        // 缓慢上升的浮尘
        ctx.globalCompositeOperation = 'lighter';
        const activeDust = Math.max(6, Math.round(dust.length * Math.min(1, density)));
        for (let i = 0; i < activeDust; i += 1) {
          const particle = dust[i];
          particle.y -= particle.speed * dt * speed * 0.35;
          particle.x += Math.sin(time * 0.5 + particle.phase) * particle.drift * dt;
          if (particle.y < -8) {
            particle.y = H + 8;
            particle.x = Math.random() * W;
          }
          if (particle.x < -8) particle.x = W + 8;
          if (particle.x > W + 8) particle.x = -8;
          ctx.globalAlpha = particle.alpha * intensity;
          ctx.fillStyle = 'rgba(200, 245, 255, 1)';
          ctx.fillRect(particle.x, particle.y, particle.size, particle.size);
        }

        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        ctx.drawImage(vignette, 0, 0, W, H);
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
