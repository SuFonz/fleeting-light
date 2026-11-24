// 群星：粒子被鼠标吸引，左键一按把它们炸开。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

bootWallpaper({
  id: 'swarm',
  defaults: { speed: 1, intensity: 1, density: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    const MAX_PARTICLES = 900;
    let particles = [];
    let pointer = { x: 0.5, y: 0.5, inside: false, down: false };
    let lastDown = false;

    function reset(particle, anywhere) {
      particle.x = anywhere ? Math.random() * W : Math.random() * W;
      particle.y = anywhere ? Math.random() * H : Math.random() * H;
      particle.vx = (Math.random() - 0.5) * 20;
      particle.vy = (Math.random() - 0.5) * 20;
      particle.size = 0.8 + Math.random() * 1.8;
      particle.tint = 160 + Math.random() * 130;  // 青 -> 紫
      return particle;
    }

    for (let i = 0; i < MAX_PARTICLES; i += 1) particles.push(reset({}, true));

    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
    ctx.fillStyle = '#05070e';
    ctx.fillRect(0, 0, W, H);

    return {
      onPointer(next) {
        const px = next.x * W;
        const py = next.y * H;
        const wasDown = lastDown;
        pointer = { x: px, y: py, inside: next.inside !== false, down: next.down === true };
        lastDown = pointer.down;

        // 按下的一瞬间：向外冲击
        if (pointer.down && !wasDown && pointer.inside) {
          for (const particle of particles) {
            const dx = particle.x - px;
            const dy = particle.y - py;
            const distance = Math.hypot(dx, dy) || 1;
            const power = Math.max(0, 520 - distance) * 0.55;
            particle.vx += (dx / distance) * power;
            particle.vy += (dy / distance) * power;
          }
        }
      },

      resize({ width: nextWidth, height: nextHeight, dpr: nextDpr }) {
        W = Math.max(1, nextWidth);
        H = Math.max(1, nextHeight);
        DPR = nextDpr;
        for (const particle of particles) reset(particle, true);
        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
        ctx.fillStyle = '#05070e';
        ctx.fillRect(0, 0, W, H);
      },

      setParams(next) {
        paramsNow = { ...paramsNow, ...next };
      },

      draw(now, dt, elapsed) {
        const speed = Number.isFinite(paramsNow.speed) ? paramsNow.speed : 1;
        const intensity = Number.isFinite(paramsNow.intensity) ? paramsNow.intensity : 1;
        const density = Number.isFinite(paramsNow.density) ? paramsNow.density : 1;

        const step = Math.min(dt, 0.05) * speed;
        const active = Math.max(60, Math.round(MAX_PARTICLES * Math.min(1, density)));
        const spring = pointer.inside ? 2.6 : 0.3;
        const damping = Math.pow(0.94, speed);
        const swirl = 0.7;                                   // 切向分量，让粒子绕着光标转
        const minRadius = Math.min(W, H) * 0.11;             // 最近只能到这个半径，避免全挤成一点
        const cx = pointer.inside ? pointer.x : W * 0.5;
        const cy = pointer.inside ? pointer.y : H * 0.5;

        ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
        ctx.globalCompositeOperation = 'source-over';
        ctx.globalAlpha = 1;
        // 拖尾（别太长，否则整屏都是残影）
        ctx.fillStyle = 'rgba(5, 7, 14, 0.30)';
        ctx.fillRect(0, 0, W, H);

        ctx.globalCompositeOperation = 'lighter';
        for (let i = 0; i < active; i += 1) {
          const particle = particles[i];

          const dx = cx - particle.x;
          const dy = cy - particle.y;
          const distance = Math.hypot(dx, dy) || 1;
          const pull = distance > minRadius ? (distance - minRadius) * spring : 0;
          const nx = dx / distance;
          const ny = dy / distance;

          particle.vx = (particle.vx + (nx * pull - ny * pull * swirl) * step) * damping;
          particle.vy = (particle.vy + (ny * pull + nx * pull * swirl) * step) * damping;
          particle.x += particle.vx * step;
          particle.y += particle.vy * step;

          if (particle.x < -40) particle.x = W + 40;
          if (particle.x > W + 40) particle.x = -40;
          if (particle.y < -40) particle.y = H + 40;
          if (particle.y > H + 40) particle.y = -40;

          const velocity = Math.hypot(particle.vx, particle.vy);
          const alpha = Math.min(0.95, (0.42 + velocity * 0.0025) * intensity);
          ctx.globalAlpha = alpha;
          ctx.fillStyle = `hsl(${particle.tint}, 95%, ${62 + Math.min(24, velocity * 0.05)}%)`;
          ctx.beginPath();
          ctx.arc(particle.x, particle.y, particle.size, 0, Math.PI * 2);
          ctx.fill();
        }

        // 光标处的一点核心亮度
        if (pointer.inside) {
          const glow = ctx.createRadialGradient(cx, cy, 0, cx, cy, 120);
          glow.addColorStop(0, `rgba(190, 245, 255, ${0.22 * intensity})`);
          glow.addColorStop(0.45, `rgba(150, 210, 255, ${0.07 * intensity})`);
          glow.addColorStop(1, 'rgba(180, 240, 255, 0)');
          ctx.globalAlpha = 1;
          ctx.fillStyle = glow;
          ctx.fillRect(cx - 120, cy - 120, 240, 240);
        }

        ctx.globalAlpha = 1;
        ctx.globalCompositeOperation = 'source-over';
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
