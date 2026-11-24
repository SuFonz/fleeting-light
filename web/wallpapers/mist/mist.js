// 雾山：晨雾里的层叠山脊，山线用正弦叠加生成，层间做视差与雾化。
import { bootWallpaper } from '../../shared/wallpaper-runtime.js';

bootWallpaper({
  id: 'mist',
  defaults: { speed: 1, intensity: 1, density: 1 },

  create({ ctx, width, height, dpr, params }) {
    let W = Math.max(1, width);
    let H = Math.max(1, height);
    let DPR = dpr;
    let paramsNow = { ...params };

    let sky = null;
    let layers = [];
    let fogBands = [];
    let fogSprite = null;
    let birds = [];

    function buildScene() {
      const horizon = H * 0.72;

      sky = document.createElement('canvas');
      sky.width = Math.floor(W * DPR);
      sky.height = Math.floor(H * DPR);
      const s = sky.getContext('2d');
      s.scale(DPR, DPR);

      const gradient = s.createLinearGradient(0, 0, 0, H);
      gradient.addColorStop(0, '#0b1230');
      gradient.addColorStop(0.35, '#22305c');
      gradient.addColorStop(0.62, '#6b5a7e');
      gradient.addColorStop(0.8, '#c4776f');
      gradient.addColorStop(1, '#f0a878');
      s.fillStyle = gradient;
      s.fillRect(0, 0, W, H);

      // 星点（只在天顶附近）
      for (let i = 0; i < Math.round((W * H) / 24000); i += 1) {
        const x = Math.random() * W;
        const y = Math.random() * H * 0.34;
        s.fillStyle = `rgba(220, 232, 255, ${0.10 + Math.random() * 0.45})`;
        s.fillRect(x, y, 1, 1);
      }

      // 太阳与它的光晕
      const sunX = W * 0.68;
      const sunY = H * 0.6;
      const sunRadius = Math.min(W, H) * 0.055;
      const halo = s.createRadialGradient(sunX, sunY, sunRadius * 0.4, sunX, sunY, Math.min(W, H) * 0.55);
      halo.addColorStop(0, 'rgba(255, 226, 178, 0.55)');
      halo.addColorStop(0.25, 'rgba(255, 176, 130, 0.20)');
      halo.addColorStop(1, 'rgba(255, 150, 120, 0)');
      s.fillStyle = halo;
      s.fillRect(0, 0, W, H);

      s.beginPath();
      s.arc(sunX, sunY, sunRadius, 0, Math.PI * 2);
      s.fillStyle = 'rgba(255, 244, 214, 0.95)';
      s.fill();

      // 山脊层：由远到近
      const specs = [
        { base: horizon - H * 0.045, amp: H * 0.045, color: '#4a4a78', alpha: 0.55, drift: 0.012, waves: 3, freq: 0.0016 },
        { base: horizon + H * 0.01, amp: H * 0.06, color: '#333a63', alpha: 0.75, drift: 0.02, waves: 4, freq: 0.0021 },
        { base: horizon + H * 0.09, amp: H * 0.075, color: '#222843', alpha: 0.9, drift: 0.03, waves: 3, freq: 0.0026 },
        { base: horizon + H * 0.19, amp: H * 0.085, color: '#151a2d', alpha: 1, drift: 0.045, waves: 4, freq: 0.0032 },
        { base: horizon + H * 0.3, amp: H * 0.07, color: '#0a0d17', alpha: 1, drift: 0.06, waves: 3, freq: 0.0038 },
      ];

      layers = specs.map((spec, index) => {
        const waves = [];
        for (let i = 0; i < spec.waves; i += 1) {
          waves.push({
            amp: spec.amp / (i + 1.2),
            freq: spec.freq * (i + 1),
            phase: index * 1.7 + i * 2.3,
            drift: spec.drift * (i % 2 === 0 ? 1 : -0.7),
          });
        }
        return { ...spec, waves };
      });

      // 雾带：预渲染一张柔和光带，逐帧缩放绘制（比每帧画矩形渐变软得多）
      fogSprite = document.createElement('canvas');
      fogSprite.width = 256;
      fogSprite.height = 128;
      const fogCtx = fogSprite.getContext('2d');
      const fogGradient = fogCtx.createRadialGradient(128, 64, 0, 128, 64, 128);
      fogGradient.addColorStop(0, 'rgba(255, 226, 205, 0.85)');
      fogGradient.addColorStop(0.45, 'rgba(255, 212, 198, 0.35)');
      fogGradient.addColorStop(1, 'rgba(255, 200, 180, 0)');
      fogCtx.save();
      fogCtx.translate(128, 64);
      fogCtx.scale(1, 0.38);
      fogCtx.translate(-128, -64);
      fogCtx.fillStyle = fogGradient;
      fogCtx.fillRect(0, 0, 256, 128);
      fogCtx.restore();

      fogBands = Array.from({ length: 7 }, (_, index) => ({
        y: horizon - H * 0.08 + index * H * 0.05,
        height: H * (0.10 + Math.random() * 0.09),
        width: W * (0.85 + Math.random() * 0.5),
        speed: (0.006 + Math.random() * 0.012) * (index % 2 === 0 ? 1 : -1),
        alpha: 0.18 + Math.random() * 0.18,
        phase: Math.random() * 10,
      }));

      // 远处几只鸟
      birds = Array.from({ length: 5 }, () => ({
        x: Math.random() * W,
        y: H * (0.28 + Math.random() * 0.22),
        size: 3 + Math.random() * 4,
        speed: 8 + Math.random() * 12,
        flap: Math.random() * 6,
      }));
    }

    function ridgeY(layer, x, t) {
      let y = layer.base;
      for (const wave of layer.waves) {
        y += Math.sin(x * wave.freq + wave.phase + t * wave.drift) * wave.amp;
      }
      return y;
    }

    buildScene();

    return {
      resize({ width: nextWidth, height: nextHeight, dpr: nextDpr }) {
        W = Math.max(1, nextWidth);
        H = Math.max(1, nextHeight);
        DPR = nextDpr;
        buildScene();
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

        // 鸟
        ctx.strokeStyle = 'rgba(24, 26, 40, 0.65)';
        ctx.lineWidth = 1.2;
        for (const bird of birds) {
          bird.x += bird.speed * dt * speed;
          if (bird.x > W + 30) {
            bird.x = -30;
            bird.y = H * (0.28 + Math.random() * 0.22);
          }
          const flap = Math.sin(now * 0.006 + bird.flap) * bird.size * 0.55;
          ctx.beginPath();
          ctx.moveTo(bird.x - bird.size, bird.y + flap);
          ctx.quadraticCurveTo(bird.x, bird.y - flap * 0.4, bird.x + bird.size, bird.y + flap);
          ctx.stroke();
        }

        // 山脊（由远到近）
        for (const layer of layers) {
          ctx.beginPath();
          ctx.moveTo(0, H);
          const stepSize = Math.max(10, W / 90);
          for (let x = 0; x <= W + stepSize; x += stepSize) {
            ctx.lineTo(Math.min(x, W), ridgeY(layer, x, t));
          }
          ctx.lineTo(W, H);
          ctx.closePath();
          ctx.globalAlpha = layer.alpha;
          ctx.fillStyle = layer.color;
          ctx.fill();
        }
        ctx.globalAlpha = 1;

        // 雾带
        ctx.globalCompositeOperation = 'lighter';
        ctx.globalAlpha = 1;
        for (const band of fogBands) {
          const offset = Math.sin(t * band.speed * 6 + band.phase) * W * 0.10;
          ctx.globalAlpha = Math.min(1, band.alpha * intensity * Math.min(1.4, density));
          ctx.drawImage(fogSprite, offset - band.width * 0.5, band.y - band.height * 0.5, band.width, band.height);
        }

        ctx.globalAlpha = 1;
        ctx.globalCompositeOperation = 'source-over';
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      },
    };
  },
});
