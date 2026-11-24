// 霓虹：赛博日落 + 透视网格，静态天空预渲染，网格每帧重画。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

bootWallpaper({
  id: 'neon',
  defaults: { speed: 1, intensity: 1, density: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    let sky = null;
    let horizon = 0;

    const GRID_COLOR = '#ff3d8b';
    const GRID_GLOW = 'rgba(255, 61, 139, 0.22)';
    const HORIZON_GLOW = 'rgba(255, 122, 199, 0.55)';

    function buildSky() {
      horizon = Math.round(H * 0.58);
      sky = document.createElement('canvas');
      sky.width = Math.floor(W * DPR);
      sky.height = Math.floor(H * DPR);
      const s = sky.getContext('2d');
      s.scale(DPR, DPR);

      const gradient = s.createLinearGradient(0, 0, 0, horizon);
      gradient.addColorStop(0, '#12031f');
      gradient.addColorStop(0.45, '#3a0b4f');
      gradient.addColorStop(0.78, '#8b1a63');
      gradient.addColorStop(1, '#ff5a7a');
      s.fillStyle = gradient;
      s.fillRect(0, 0, W, horizon);

      // 星星
      for (let i = 0; i < Math.round((W * horizon) / 6400); i += 1) {
        const x = Math.random() * W;
        const y = Math.random() * horizon * 0.72;
        s.fillStyle = `rgba(255, 235, 255, ${0.15 + Math.random() * 0.55})`;
        s.fillRect(x, y, 1, 1);
      }

      // 太阳：带横向切缝
      const sunRadius = Math.min(W, H) * 0.19;
      const sunX = W * 0.5;
      const sunY = horizon - sunRadius * 0.42;
      const sunGradient = s.createLinearGradient(0, sunY - sunRadius, 0, sunY + sunRadius);
      sunGradient.addColorStop(0, '#fff4a8');
      sunGradient.addColorStop(0.45, '#ffb43d');
      sunGradient.addColorStop(1, '#ff2d95');

      s.save();
      s.beginPath();
      s.arc(sunX, sunY, sunRadius, 0, Math.PI * 2);
      s.clip();
      s.fillStyle = sunGradient;
      s.fillRect(sunX - sunRadius, sunY - sunRadius, sunRadius * 2, sunRadius * 2);
      // 切缝
      const stripes = 9;
      for (let i = 0; i < stripes; i += 1) {
        const t = i / stripes;
        const y = sunY - sunRadius * 0.1 + t * sunRadius * 1.1;
        s.fillStyle = 'rgba(20, 4, 28, 0.85)';
        s.fillRect(sunX - sunRadius, y, sunRadius * 2, 1.5 + t * 5);
      }
      s.restore();

      // 光晕
      const glow = s.createRadialGradient(sunX, sunY, sunRadius * 0.6, sunX, sunY, sunRadius * 3.4);
      glow.addColorStop(0, 'rgba(255, 96, 150, 0.30)');
      glow.addColorStop(1, 'rgba(255, 96, 150, 0)');
      s.fillStyle = glow;
      s.fillRect(0, 0, W, horizon);

      // 地面底色
      const ground = s.createLinearGradient(0, horizon, 0, H);
      ground.addColorStop(0, '#1a0324');
      ground.addColorStop(1, '#06010c');
      s.fillStyle = ground;
      s.fillRect(0, horizon, W, H - horizon);
    }

    buildSky();

    return {
      resize({ width: nextWidth, height: nextHeight, dpr: nextDpr }) {
        W = Math.max(1, nextWidth);
        H = Math.max(1, nextHeight);
        DPR = nextDpr;
        buildSky();
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
        ctx.drawImage(sky, 0, 0, W, H);

        ctx.globalCompositeOperation = 'lighter';

        // 地平线辉光
        const glowAlpha = (0.28 * intensity).toFixed(3);
        const horizonGlow = ctx.createLinearGradient(0, horizon - 90, 0, horizon + 40);
        horizonGlow.addColorStop(0, 'rgba(255, 96, 150, 0)');
        horizonGlow.addColorStop(0.65, `rgba(255, 122, 199, ${glowAlpha})`);
        horizonGlow.addColorStop(1, 'rgba(255, 96, 150, 0)');
        ctx.fillStyle = horizonGlow;
        ctx.fillRect(0, horizon - 90, W, 130);

        // 网格：横向线由远及近加速滚动
        const phase = (t * 0.35) % 1;
        const rows = Math.round(22 * Math.min(1.6, density));
        ctx.strokeStyle = GRID_COLOR;
        for (let i = 0; i < rows; i += 1) {
          const k = i + phase;
          const y = horizon + Math.pow(k, 1.85) * (H - horizon) * 0.026;
          if (y > H) continue;
          const fade = Math.min(1, (y - horizon) / ((H - horizon) * 0.55));
          ctx.globalAlpha = Math.min(0.85, 0.08 + fade * 0.75) * intensity;
          ctx.lineWidth = 1 + fade * 1.6;
          ctx.beginPath();
          ctx.moveTo(0, y);
          ctx.lineTo(W, y);
          ctx.stroke();
        }

        // 纵向线：从灭点发散
        const cx = W * 0.5;
        const columns = Math.round(18 * Math.min(1.6, density));
        ctx.globalAlpha = 0.5 * intensity;
        ctx.lineWidth = 1.1;
        for (let i = -columns; i <= columns; i += 1) {
          const spread = (i / columns) * W * 0.95;
          ctx.beginPath();
          ctx.moveTo(cx + spread * 0.02, horizon);
          ctx.lineTo(cx + spread, H + 40);
          ctx.stroke();
        }

        // 网格整体泛光
        ctx.globalAlpha = 0.16 * intensity;
        ctx.strokeStyle = GRID_GLOW;
        ctx.lineWidth = 3;
        for (let i = 0; i < rows; i += 1) {
          const k = i + phase;
          const y = horizon + Math.pow(k, 1.85) * (H - horizon) * 0.026;
          if (y > H) continue;
          ctx.beginPath();
          ctx.moveTo(0, y);
          ctx.lineTo(W, y);
          ctx.stroke();
        }

        ctx.globalAlpha = 1;
        ctx.globalCompositeOperation = 'source-over';
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
