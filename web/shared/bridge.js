// 统一桥接层：前端只认 send(json) / onMessage(cb)，不关心底下是 WebView2 还是 webview/webview。
//
// - Windows / WebView2 : window.chrome.webview.postMessage / 'message' 事件
// - webview/webview    : window.ipc.postMessage（预留，Linux 后端也可以适配这套）
(function (global) {
  'use strict';

  const listeners = new Set();
  let transport = null;

  function normalize(raw) {
    if (typeof raw === 'string') {
      try {
        return JSON.parse(raw);
      } catch (error) {
        return { type: 'raw', data: raw };
      }
    }
    return raw;
  }

  function dispatch(raw) {
    const message = normalize(raw);
    for (const listener of Array.from(listeners)) {
      try {
        listener(message);
      } catch (error) {
        console.error('[bridge] 监听器异常', error);
      }
    }
  }

  function onMessage(handler) {
    listeners.add(handler);
    return () => listeners.delete(handler);
  }

  function send(message) {
    if (!transport) {
      console.warn('[bridge] 没有可用的通信通道', message);
      return;
    }
    transport.send(JSON.stringify(message));
  }

  if (global.chrome && global.chrome.webview) {
    global.chrome.webview.addEventListener('message', (event) => dispatch(event.data));
    transport = {
      name: 'webview2',
      send: (text) => global.chrome.webview.postMessage(text),
    };
  } else if (global.ipc && typeof global.ipc.postMessage === 'function') {
    global.addEventListener('message', (event) => dispatch(event.data));
    transport = {
      name: 'webview',
      send: (text) => global.ipc.postMessage(text),
    };
  }
  // window.external（XAML WebView 那条路）已从 Chromium 弃用，本项目也用不到它。
  // 将来真要接，在这里加一个分支即可。

  global.bridge = {
    send,
    onMessage,
    dispatch,  // 离线调试用：bridge.dispatch({type:'init', ...})
    get platform() {
      return transport ? transport.name : 'none';
    },
    get connected() {
      return transport !== null;
    },
  };
})(window);
