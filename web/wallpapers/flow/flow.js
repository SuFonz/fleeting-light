// 流光：粒子沿矢量场流动，靠残影累积出丝带般的纹理。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

const PALETTE = [
  'rgba(92, 246, 214, 1)',
  'rgba(120, 232, 255, 1)',
  'rgba(140, 170, 255, 1)',
  'rgba(172, 140, 255, 1)',
  'rgba(232, 130, 255, 1)',
  'rgba(110, 255, 190, 1)',
];

bootWallpaper({
  id: 'flow',
  defaults: { speed: 1, intensity: 1, density: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    const MAX_PARTICLES = 1100;
    const particles = [];
    // 按颜色分组攒线段，一次 stroke 画完一组，把 1100 次 draw call 压到 6 次
    const buckets = PALETTE.map(() => []);

    function seed(particle, anywhere) {
      particle.x = Math.random() * W;
      particle.y = Math.random() * H;
      particle.age = anywhere ? Math.random() * 4 : 0;
      particle.colorIndex = (Math.random() * PALETTE.length) | 0;
      return particle;
    }

    for (let i = 0; i < MAX_PARTICLES; i += 1) particles.push(seed({}, true));

    // 便宜的伪 curl 场：三个不同频率的正弦叠加
    function angleAt(x, y, t) {
      const nx = x * 0.0016;
      const ny = y * 0.0021;
      const a = Math.sin(nx * 3.1 + t * 0.42) + Math.cos(ny * 2.7 - t * 0.33);
      const b = Math.sin((nx + ny) * 1.9 + t * 0.19);
      return (a + b * 0.8) * Math.PI * 0.55;
    }

    function clearCanvas() {
      ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
      ctx.globalCompositeOperation = 'source-over';
      ctx.globalAlpha = 1;
      ctx.fillStyle = '#04060c';
      ctx.fillRect(0, 0, W, H);
    }

    clearCanvas();

    return {
      resize({ width: nextWidth, height: nextHeight, dpr: nextDpr }) {
        W = Math.max(1, nextWidth);
        H = Math.max(1, nextHeight);
        DPR = nextDpr;
        clearCanvas();
        for (const particle of particles) seed(particle, true);
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
        // 残影：每帧只擦掉一点点
        ctx.fillStyle = 'rgba(4, 6, 12, 0.032)';
        ctx.fillRect(0, 0, W, H);

        ctx.globalCompositeOperation = 'lighter';
        ctx.lineCap = 'round';

        const active = Math.max(60, Math.round(MAX_PARTICLES * Math.min(1, density)));
        const step = 64 * speed;
        const margin = 40;

        // 先把这一帧所有线段按颜色攒进对应的桶里
        for (const bucket of buckets) bucket.length = 0;
        for (let i = 0; i < active; i += 1) {
          const particle = particles[i];
          const angle = angleAt(particle.x, particle.y, t);
          const nx = particle.x + Math.cos(angle) * step * dt;
          const ny = particle.y + Math.sin(angle) * step * dt;

          const bucket = buckets[particle.colorIndex];
          bucket.push(particle.x, particle.y, nx, ny);

          particle.x = nx;
          particle.y = ny;
          particle.age += dt;

          if (particle.x < -margin || particle.x > W + margin || particle.y < -margin || particle.y > H + margin) {
            seed(particle, false);
          } else if (particle.age > 8 + Math.random() * 6) {
            seed(particle, false);
          }
        }

        // 每种颜色一次 stroke：1100 次 draw call 降到 6 次
        ctx.globalAlpha = 0.62 * intensity;
        ctx.lineWidth = 1.0;
        for (let c = 0; c < buckets.length; c += 1) {
          const bucket = buckets[c];
          if (bucket.length === 0) continue;
          ctx.strokeStyle = PALETTE[c];
          ctx.beginPath();
          for (let k = 0; k < bucket.length; k += 4) {
            ctx.moveTo(bucket[k], bucket[k + 1]);
            ctx.lineTo(bucket[k + 2], bucket[k + 3]);
          }
          ctx.stroke();
        }

        ctx.globalAlpha = 1;
        ctx.globalCompositeOperation = 'source-over';
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
