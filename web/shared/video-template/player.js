// 视频播放后端（抽象层）。
//
// 上层页面只依赖这里导出的 createPlayer()，不关心底层怎么解码：
//   - 现在：HTML5 <video>，由 WebView 自己的解码器播放
//   - 将来：换成 ffmpeg.wasm（逐帧喂给 canvas）或 libmpv（原生渲染后贴图）
//     只需要提供同样的接口，页面代码一行都不用改。
//
// 统一接口：
//   play() / pause() / paused() / setRate(v) / setVolume(v) / currentTime() / destroy()

export const BACKEND_KIND = 'html5-video';

export function createPlayer({ host, src, onReady, onError }) {
    const el = document.createElement('video');
    el.className = 'video-layer';
    el.src = src;
    el.muted = true;        // 壁纸不该出声
    el.loop = true;
    el.playsInline = true;
    el.preload = 'auto';
    el.disablePictureInPicture = true;

    el.addEventListener('loadeddata', () => {
        if (onReady) onReady();
    });
    el.addEventListener('error', () => {
        if (onError) onError(el.error);
    });

    host.append(el);

    return {
        kind: BACKEND_KIND,

        play() {
            const started = el.play();
            if (started && started.catch) started.catch(() => {});
        },

        pause() {
            el.pause();
        },

        paused() {
            return el.paused;
        },

        setRate(value) {
            el.playbackRate = Math.max(0.25, Math.min(4, Number(value) || 1));
        },

        setVolume(value) {
            el.volume = Math.max(0, Math.min(1, Number(value) || 0));
        },

        currentTime() {
            return el.currentTime;
        },

        destroy() {
            el.pause();
            el.removeAttribute('src');
            el.load();
            el.remove();
        },
    };
}
