// 光斑：预渲染几种柔和光球，散景漂浮 + 暗角，适合做安静的桌面底图。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

const SPRITE_COLORS = [
  [255, 205, 140],
  [255, 150, 190],
  [140, 220, 255],
  [190, 160, 255],
  [255, 250, 235],
];

bootWallpaper({
  id: 'bokeh',
  defaults: { speed: 1, intensity: 1, density: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    let sprites = [];
    let orbs = [];
    let backdrop = null;

    function buildSprites() {
      sprites = SPRITE_COLORS.map(([r, g, b]) => {
        const size = 128;
        const canvas = document.createElement('canvas');
        canvas.width = size;
        canvas.height = size;
        const s = canvas.getContext('2d');
        const gradient = s.createRadialGradient(size / 2, size / 2, 0, size / 2, size / 2, size / 2);
        gradient.addColorStop(0, `rgba(${r}, ${g}, ${b}, 1)`);
        gradient.addColorStop(0.25, `rgba(${r}, ${g}, ${b}, 0.72)`);
        gradient.addColorStop(0.5, `rgba(${r}, ${g}, ${b}, 0.34)`);
        gradient.addColorStop(0.78, `rgba(${r}, ${g}, ${b}, 0.10)`);
        gradient.addColorStop(1, `rgba(${r}, ${g}, ${b}, 0)`);
        s.fillStyle = gradient;
        s.fillRect(0, 0, size, size);
        return canvas;
      });
    }

    function buildBackdrop() {
      backdrop = document.createElement('canvas');
      backdrop.width = Math.floor(W * DPR);
      backdrop.height = Math.floor(H * DPR);
      const b = backdrop.getContext('2d');
      b.scale(DPR, DPR);

      const base = b.createLinearGradient(0, 0, W * 0.4, H);
      base.addColorStop(0, '#0d1122');
      base.addColorStop(0.55, '#12172b');
      base.addColorStop(1, '#1b1b33');
      b.fillStyle = base;
      b.fillRect(0, 0, W, H);

      // 一处暖色光源，让散景有来处
      const source = b.createRadialGradient(W * 0.72, H * 0.18, 0, W * 0.72, H * 0.18, Math.max(W, H) * 0.7);
      source.addColorStop(0, 'rgba(255, 190, 120, 0.10)');
      source.addColorStop(0.4, 'rgba(255, 150, 150, 0.04)');
      source.addColorStop(1, 'rgba(0, 0, 0, 0)');
      b.fillStyle = source;
      b.fillRect(0, 0, W, H);

      const vignette = b.createRadialGradient(W * 0.5, H * 0.5, Math.min(W, H) * 0.25, W * 0.5, H * 0.5,
        Math.max(W, H) * 0.8);
      vignette.addColorStop(0, 'rgba(0, 0, 0, 0)');
      vignette.addColorStop(1, 'rgba(0, 0, 0, 0.55)');
      b.fillStyle = vignette;
      b.fillRect(0, 0, W, H);
    }

    function buildOrbs() {
      const count = 44;
      orbs = Array.from({ length: count }, (_, index) => {
        const depth = 0.3 + Math.random() * 0.7;
        const big = index % 6 === 0;
        const radius = (big ? 110 + Math.random() * 150 : 22 + Math.random() * 85) * (0.55 + depth * 0.65);
        return {
          x: Math.random() * W,
          y: Math.random() * H,
          radius,
          sprite: (Math.random() * SPRITE_COLORS.length) | 0,
          alpha: big ? 0.10 + Math.random() * 0.10 : 0.22 + Math.random() * 0.32,
          vx: (Math.random() - 0.5) * 9 * depth,
          vy: -(4 + Math.random() * 10) * depth,
          phase: Math.random() * Math.PI * 2,
          wobble: 0.3 + Math.random() * 1.4,
        };
      });
      // 大光斑先画，作为背景散景
      orbs.sort((a, b) => b.radius - a.radius);
    }

    buildSprites();
    buildBackdrop();
    buildOrbs();

    return {
      resize({ width: nextWidth, height: nextHeight, dpr: nextDpr }) {
        W = Math.max(1, nextWidth);
        H = Math.max(1, nextHeight);
        DPR = nextDpr;
        buildBackdrop();
        buildOrbs();
      },

      setParams(next) {
        paramsNow = { ...paramsNow, ...next };
      },

      draw(now, dt, elapsed) {
        const speed = Number.isFinite(paramsNow.speed) ? paramsNow.speed : 1;
        const intensity = Number.isFinite(paramsNow.intensity) ? paramsNow.intensity : 1;
        const density = Number.isFinite(paramsNow.density) ? paramsNow.density : 1;
        const t = elapsed * speed;

        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        ctx.drawImage(backdrop, 0, 0, W, H);

        ctx.globalCompositeOperation = 'lighter';
        const active = Math.max(6, Math.round(orbs.length * Math.min(1, density)));
        for (let i = 0; i < active; i += 1) {
          const orb = orbs[i];
          orb.x += orb.vx * dt * speed;
          orb.y += orb.vy * dt * speed;
          const drift = Math.sin(t * orb.wobble + orb.phase) * 12;

          if (orb.y < -orb.radius * 2) {
            orb.y = H + orb.radius * 2;
            orb.x = Math.random() * W;
          }
          if (orb.x < -orb.radius * 2) orb.x = W + orb.radius * 2;
          if (orb.x > W + orb.radius * 2) orb.x = -orb.radius * 2;

          const diameter = orb.radius * 2;
          ctx.globalAlpha = Math.min(1, orb.alpha * intensity);
          ctx.drawImage(sprites[orb.sprite], orb.x - orb.radius + drift, orb.y - orb.radius, diameter, diameter);
        }

        ctx.globalAlpha = 1;
        ctx.globalCompositeOperation = 'source-over';
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
