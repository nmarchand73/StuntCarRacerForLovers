/**
 * Chip-only SNDH music for the web build via ym2149-wasm.
 * Native builds use libpsgplay (digi + YM where present); web is YM chip playback.
 */
import initYm2149, { Ym2149Player } from './ym2149/ym2149_wasm.js';

const YM_SAMPLE_RATE = 44100;
const GAME_MODE = {
  TRACK_MENU: 0,
  TRACK_PREVIEW: 1,
  GAME_IN_PROGRESS: 2,
  GAME_OVER: 3,
};

const TRACKS = {
  menu: {
    paths: [
      'data/Music/Menu/Blood_Money.sndh',
      '/data/Music/Menu/Blood_Money.sndh',
    ],
    subtune: 1,
    gain: 0.35,
  },
  race: {
    paths: [
      'data/Music/Race/Wings_of_Death_STe.sndh',
      '/data/Music/Race/Wings_of_Death_STe.sndh',
    ],
    subtune: 1,
    gain: 0.25,
  },
};

let ymReady = null;
let menuMusicEnabled = true;
let lastGameMode = GAME_MODE.TRACK_MENU;
let pendingRetry = false;
let retryTimer = null;

function scheduleMusicRetry() {
  if (retryTimer || !pendingRetry) {
    return;
  }
  retryTimer = window.setInterval(() => {
    if (!pendingRetry) {
      window.clearInterval(retryTimer);
      retryTimer = null;
      return;
    }
    void retryIfNeeded();
  }, 400);
}

function markPendingRetry() {
  pendingRetry = true;
  scheduleMusicRetry();
}

let audioNode = null;
let ymPlayer = null;
let ymDurationSec = null;
let ymSamplesOut = 0;
let activeTrackKey = null;

function copyOrResample(source, target) {
  if (source.length === target.length) {
    target.set(source);
    return;
  }
  if (source.length === 0) {
    target.fill(0);
    return;
  }
  const ratio = source.length / target.length;
  for (let i = 0; i < target.length; i += 1) {
    const srcIndex = i * ratio;
    const leftIndex = Math.min(Math.floor(srcIndex), source.length - 1);
    const rightIndex = Math.min(leftIndex + 1, source.length - 1);
    const mix = srcIndex - leftIndex;
    target[i] = source[leftIndex] * (1 - mix) + source[rightIndex] * mix;
  }
}

function getAudioContext() {
  if (typeof Module !== 'undefined' && Module.SDL2 && Module.SDL2.audioContext) {
    return Module.SDL2.audioContext;
  }
  return null;
}

async function waitForAudioContext(timeoutMs = 15000) {
  const started = performance.now();
  while (performance.now() - started < timeoutMs) {
    const ctx = getAudioContext();
    if (ctx) {
      return ctx;
    }
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  return null;
}

async function ensureYmReady() {
  if (!ymReady) {
    ymReady = initYm2149(new URL('./ym2149/ym2149_wasm_bg.wasm', import.meta.url)).then(() => undefined);
  }
  await ymReady;
}

function getEmscriptenFs() {
  if (typeof Module !== 'undefined' && Module.FS && Module.FS.readFile) {
    return Module.FS;
  }
  if (typeof FS !== 'undefined' && FS.readFile) {
    return FS;
  }
  return null;
}

function readSndhFromMemfs(paths) {
  const fs = getEmscriptenFs();
  if (!fs) {
    return null;
  }
  for (const path of paths) {
    try {
      const bytes = fs.readFile(path);
      if (bytes && bytes.length > 0) {
        return bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
      }
    } catch (error) {
      // Try the next mounted path.
    }
  }
  return null;
}

async function fetchSndh(paths) {
  const fromFs = readSndhFromMemfs(paths);
  if (fromFs) {
    return fromFs;
  }

  for (const path of paths) {
    const url = path.startsWith('/') ? path.slice(1) : path;
    try {
      const response = await fetch(url);
      if (response.ok) {
        return new Uint8Array(await response.arrayBuffer());
      }
    } catch (error) {
      // Try the next URL.
    }
  }

  throw new Error(`Failed to load SNDH: ${paths.join(', ')}`);
}

function stopPlayback() {
  if (audioNode) {
    audioNode.disconnect();
    audioNode.onaudioprocess = null;
    audioNode = null;
  }
  if (ymPlayer) {
    ymPlayer.stop();
    ymPlayer.free();
    ymPlayer = null;
  }
  ymDurationSec = null;
  ymSamplesOut = 0;
  activeTrackKey = null;
}

function trackKeyForMode(mode) {
  if (mode === GAME_MODE.GAME_IN_PROGRESS) {
    return 'race';
  }
  if (
    mode === GAME_MODE.TRACK_MENU ||
    mode === GAME_MODE.TRACK_PREVIEW ||
    mode === GAME_MODE.GAME_OVER
  ) {
    return menuMusicEnabled ? 'menu' : null;
  }
  return null;
}

async function startTrack(trackKey) {
  if (activeTrackKey === trackKey && ymPlayer && audioNode) {
    return true;
  }

  stopPlayback();
  if (!trackKey) {
    pendingRetry = false;
    return true;
  }

  const spec = TRACKS[trackKey];
  if (!spec) {
    return false;
  }

  await ensureYmReady();

  const ctx = await waitForAudioContext();
  if (!ctx) {
    console.warn('SCRWebMusic: waiting for SDL audio context');
    markPendingRetry();
    return false;
  }

  if (ctx.state === 'suspended') {
    markPendingRetry();
    return false;
  }

  const sndhBytes = await fetchSndh(spec.paths);
  const player = new Ym2149Player(sndhBytes);
  ymPlayer = player;

  const subtune = spec.subtune;
  if (player.subsongCount() >= subtune) {
    player.setSubsong(subtune);
  } else {
    player.setSubsong(1);
  }

  player.set_volume(spec.gain);
  player.set_color_filter(true);
  player.play();

  const rateHz = player.metadata.frame_rate > 0 ? player.metadata.frame_rate : 50;
  const frameCount = player.frame_count() || player.metadata.frame_count;
  ymDurationSec =
    player.metadata.duration_seconds > 0
      ? player.metadata.duration_seconds
      : frameCount > 0
        ? frameCount / rateHz
        : null;
  ymSamplesOut = 0;
  activeTrackKey = trackKey;
  pendingRetry = false;

  const bufferSize = 2048;
  const scriptNode = ctx.createScriptProcessor(bufferSize, 0, 2);
  scriptNode.onaudioprocess = (event) => {
    const left = event.outputBuffer.getChannelData(0);
    const right = event.outputBuffer.getChannelData(1);
    const current = ymPlayer;
    if (!current || !current.is_playing()) {
      left.fill(0);
      right.fill(0);
      return;
    }

    const outputRate = ctx.sampleRate || YM_SAMPLE_RATE;
    const want = Math.max(1, Math.round((left.length * YM_SAMPLE_RATE) / outputRate));
    const samples = current.generateSamples(want);
    copyOrResample(samples, left);
    right.set(left);
    ymSamplesOut += samples.length;

    if (ymDurationSec != null && ymSamplesOut / YM_SAMPLE_RATE >= ymDurationSec) {
      current.restart();
      ymSamplesOut = 0;
    }
  };

  scriptNode.connect(ctx.destination);
  audioNode = scriptNode;
  console.log(`SCRWebMusic: playing ${trackKey} (subtune ${subtune})`);
  return true;
}

async function applyGameMode(mode) {
  lastGameMode = mode;
  const trackKey = trackKeyForMode(mode);
  try {
    const ok = await startTrack(trackKey);
    if (!ok) {
      markPendingRetry();
    }
  } catch (error) {
    console.error('SCRWebMusic:', error);
    markPendingRetry();
    stopPlayback();
  }
}

async function retryIfNeeded() {
  if (!pendingRetry && activeTrackKey === trackKeyForMode(lastGameMode)) {
    return;
  }
  await applyGameMode(lastGameMode);
}

async function drainScrMusicQueue() {
  const queue = window.__scrMusicQueue || [];
  window.__scrMusicQueue = [];
  for (const item of queue) {
    const fn = SCRWebMusicImpl[item.method];
    if (typeof fn === 'function') {
      await fn.apply(SCRWebMusicImpl, item.args);
    }
  }
}

const SCRWebMusicImpl = {
  async init() {
    await ensureYmReady();
    await applyGameMode(GAME_MODE.TRACK_MENU);
  },

  shutdown() {
    stopPlayback();
    pendingRetry = false;
  },

  setGameMode(mode) {
    void applyGameMode(mode);
  },

  _isMenuMusicEnabled() {
    return menuMusicEnabled;
  },

  setMenuMusicEnabled(enabled) {
    menuMusicEnabled = !!enabled;
    void applyGameMode(lastGameMode);
  },

  async _resumeAudioContext() {
    const ctx = getAudioContext();
    if (ctx && ctx.state === 'suspended') {
      await ctx.resume();
    }
    await retryIfNeeded();
  },

  async onAudioUnlocked() {
    await retryIfNeeded();
  },
};

window.SCRWebMusic = {
  __ready: true,
  init() {
    return SCRWebMusicImpl.init();
  },
  shutdown() {
    SCRWebMusicImpl.shutdown();
  },
  setGameMode(mode) {
    SCRWebMusicImpl.setGameMode(mode);
  },
  isMenuMusicEnabled() {
    return SCRWebMusicImpl._isMenuMusicEnabled();
  },
  setMenuMusicEnabled(enabled) {
    SCRWebMusicImpl.setMenuMusicEnabled(enabled);
  },
  resumeAudioContext() {
    return SCRWebMusicImpl._resumeAudioContext();
  },
  onAudioUnlocked() {
    return SCRWebMusicImpl.onAudioUnlocked();
  },
};

void drainScrMusicQueue();
