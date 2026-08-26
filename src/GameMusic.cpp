#include "platform_sdl_gl.h"
#include "GameMusic.h"

#include <cstdio>
#include <deque>
#include <vector>

#if defined(STUNT_PSGPLAY_MUSIC)

extern "C" {
#include <psgplay/psgplay.h>
#include <psgplay/stereo.h>
}

static const char* kMenuMusicPath = "data/Music/Menu/Blood_Money.sndh";
static const char* kRaceMusicPath = "data/Music/Race/Goldrunner.sndh";
static const int kMenuSubtune = 1;
static const int kRaceSubtune = 1; /* Gold Runner: digi + YM */
static const int kSampleRate = 44100;
static const size_t kPumpChunkFrames = 4096;
static const size_t kTargetQueuedFrames = 22050; /* ~0.5 s */
static const size_t kMaxQueuedFrames = 88200;  /* ~2 s */

static std::vector<uint8_t> g_menu_sndh;
static std::vector<uint8_t> g_race_sndh;
static const std::vector<uint8_t>* g_active_sndh = NULL;
static int g_active_subtune = 1;
static struct psgplay* g_psgplay = NULL;
static SDL_mutex* g_music_mutex = NULL;
static std::deque<float> g_music_queue;
static const float kMenuMusicGain = 0.35f;
static const float kRaceMusicGain = 0.45f * 0.4f; /* 60% quieter than menu-relative race mix */
static bool g_ready = false;
static bool g_menu_music_enabled = true;
static GameModeType g_last_music_mode = TRACK_MENU;
static float g_music_gain = kMenuMusicGain;

static bool LoadBinaryFile(const char* path, std::vector<uint8_t>& out) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("GameMusic: failed to open %s\n", path);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    const long size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return false;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    out.resize(static_cast<size_t>(size));
    const size_t read_bytes = fread(out.data(), 1, out.size(), file);
    fclose(file);
    if (read_bytes != out.size()) {
        out.clear();
        return false;
    }

    return true;
}

static void StopPlaybackLocked() {
    if (g_psgplay) {
        psgplay_free(g_psgplay);
        g_psgplay = NULL;
    }
    g_music_queue.clear();
}

static bool StartPlaybackLocked(const std::vector<uint8_t>& data, int subtune) {
    StopPlaybackLocked();
    g_active_sndh = &data;
    g_active_subtune = subtune;
    g_psgplay = psgplay_init(data.data(), data.size(), subtune, kSampleRate);
    if (!g_psgplay) {
        printf("GameMusic: psgplay_init failed for subtune %d\n", subtune);
        g_active_sndh = NULL;
        return false;
    }
    return true;
}

static void QueueStereoChunk(const psgplay_stereo* samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        while (g_music_queue.size() / 2 >= kMaxQueuedFrames) {
            g_music_queue.pop_front();
            g_music_queue.pop_front();
        }
        g_music_queue.push_back(static_cast<float>(samples[i].left) / 32768.0f);
        g_music_queue.push_back(static_cast<float>(samples[i].right) / 32768.0f);
    }
}

static void RestartCurrentTrackLocked() {
    if (!g_active_sndh) {
        return;
    }
    StartPlaybackLocked(*g_active_sndh, g_active_subtune);
}

static void PumpLocked() {
    if (!g_psgplay) {
        return;
    }

    psgplay_stereo chunk[kPumpChunkFrames];
    while (g_music_queue.size() / 2 < kTargetQueuedFrames) {
        const ssize_t read_count = psgplay_read_stereo(g_psgplay, chunk, kPumpChunkFrames);
        if (read_count < 0) {
            printf("GameMusic: psgplay_read_stereo failed\n");
            StopPlaybackLocked();
            return;
        }
        if (read_count == 0) {
            RestartCurrentTrackLocked();
            return;
        }
        QueueStereoChunk(chunk, static_cast<size_t>(read_count));
    }
}

static void ApplyMusicForMode(GameModeType mode) {
    g_last_music_mode = mode;

    if (mode == GAME_IN_PROGRESS) {
        g_music_gain = kRaceMusicGain;
        if (!StartPlaybackLocked(g_race_sndh, kRaceSubtune)) {
            printf("GameMusic: race music unavailable\n");
        }
        return;
    }

    if (mode == TRACK_MENU || mode == TRACK_PREVIEW || mode == GAME_OVER) {
        if (!g_menu_music_enabled) {
            StopPlaybackLocked();
            g_active_sndh = NULL;
            return;
        }
        g_music_gain = kMenuMusicGain;
        if (!StartPlaybackLocked(g_menu_sndh, kMenuSubtune)) {
            printf("GameMusic: menu music unavailable\n");
        }
    }
}

void GameMusic_Init(void) {
    if (g_ready) {
        return;
    }

    if (!LoadBinaryFile(kMenuMusicPath, g_menu_sndh)) {
        return;
    }
    if (!LoadBinaryFile(kRaceMusicPath, g_race_sndh)) {
        g_menu_sndh.clear();
        return;
    }

    g_music_mutex = SDL_CreateMutex();
    if (!g_music_mutex) {
        g_menu_sndh.clear();
        g_race_sndh.clear();
        return;
    }

    g_ready = true;
    printf("GameMusic: loaded menu and race SNDH tracks\n");
    ApplyMusicForMode(TRACK_MENU);
}

void GameMusic_Shutdown(void) {
    if (!g_ready) {
        return;
    }

    if (g_music_mutex) {
        SDL_LockMutex(g_music_mutex);
        StopPlaybackLocked();
        g_active_sndh = NULL;
        SDL_UnlockMutex(g_music_mutex);
        SDL_DestroyMutex(g_music_mutex);
        g_music_mutex = NULL;
    }

    g_menu_sndh.clear();
    g_race_sndh.clear();
    g_ready = false;
}

void GameMusic_Pump(void) {
    if (!g_ready || !g_music_mutex) {
        return;
    }

    SDL_LockMutex(g_music_mutex);
    PumpLocked();
    SDL_UnlockMutex(g_music_mutex);
}

void GameMusic_OnGameModeChanged(GameModeType prev, GameModeType next) {
    (void)prev;
    if (!g_ready || !g_music_mutex) {
        return;
    }

    SDL_LockMutex(g_music_mutex);
    ApplyMusicForMode(next);
    PumpLocked();
    SDL_UnlockMutex(g_music_mutex);
}

bool GameMusic_IsMenuMusicEnabled(void) {
    return g_menu_music_enabled;
}

void GameMusic_SetMenuMusicEnabled(bool enabled) {
    if (g_menu_music_enabled == enabled) {
        return;
    }

    g_menu_music_enabled = enabled;
    if (!g_ready || !g_music_mutex) {
        return;
    }

    SDL_LockMutex(g_music_mutex);
    if (enabled) {
        ApplyMusicForMode(g_last_music_mode);
        PumpLocked();
    } else if (g_last_music_mode != GAME_IN_PROGRESS) {
        StopPlaybackLocked();
        g_active_sndh = NULL;
    }
    SDL_UnlockMutex(g_music_mutex);
    printf("Menu music: %s\n", enabled ? "On" : "Off");
}

void GameMusic_ToggleMenuMusic(void) {
    GameMusic_SetMenuMusicEnabled(!g_menu_music_enabled);
}

void GameMusic_Mix(float* out, int frame_count, int channels) {
    if (!g_ready || !g_music_mutex || !out || frame_count <= 0) {
        return;
    }

    SDL_LockMutex(g_music_mutex);
    for (int frame = 0; frame < frame_count; ++frame) {
        float left = 0.0f;
        float right = 0.0f;
        if (g_music_queue.size() >= 2) {
            left = g_music_queue.front() * g_music_gain;
            g_music_queue.pop_front();
            right = g_music_queue.front() * g_music_gain;
            g_music_queue.pop_front();
        }

        const int base = frame * channels;
        if (channels == 1) {
            out[base] += (left + right) * 0.5f;
        } else {
            out[base] += left;
            out[base + 1] += right;
        }
    }
    SDL_UnlockMutex(g_music_mutex);
}

#else /* !STUNT_PSGPLAY_MUSIC */

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>

EM_JS(void, js_web_music_init, (), {
    if (typeof SCRWebMusic !== 'undefined' && SCRWebMusic.init) {
        SCRWebMusic.init().catch(function (err) { console.error('SCRWebMusic init failed', err); });
    }
});

EM_JS(void, js_web_music_shutdown, (), {
    if (typeof SCRWebMusic !== 'undefined' && SCRWebMusic.shutdown) SCRWebMusic.shutdown();
});

EM_JS(void, js_web_music_set_mode, (int mode), {
    if (typeof SCRWebMusic !== 'undefined' && SCRWebMusic.setGameMode) SCRWebMusic.setGameMode(mode);
});

EM_JS(int, js_web_music_is_menu_enabled, (), {
    if (typeof SCRWebMusic !== 'undefined' && SCRWebMusic.isMenuMusicEnabled)
        return SCRWebMusic.isMenuMusicEnabled() ? 1 : 0;
    return 1;
});

EM_JS(void, js_web_music_set_menu_enabled, (int enabled), {
    if (typeof SCRWebMusic !== 'undefined' && SCRWebMusic.setMenuMusicEnabled)
        SCRWebMusic.setMenuMusicEnabled(!!enabled);
});

EM_JS(void, js_web_music_resume_context, (), {
    if (typeof SCRWebMusic !== 'undefined' && SCRWebMusic.resumeAudioContext)
        SCRWebMusic.resumeAudioContext().catch(function () {});
});

void GameMusic_Init(void) {
    js_web_music_init();
}

void GameMusic_Shutdown(void) {
    js_web_music_shutdown();
}

void GameMusic_Pump(void) {}

void GameMusic_OnGameModeChanged(GameModeType prev, GameModeType next) {
    (void)prev;
    js_web_music_set_mode(static_cast<int>(next));
}

bool GameMusic_IsMenuMusicEnabled(void) {
    return js_web_music_is_menu_enabled() != 0;
}

void GameMusic_SetMenuMusicEnabled(bool enabled) {
    js_web_music_set_menu_enabled(enabled ? 1 : 0);
}

void GameMusic_ToggleMenuMusic(void) {
    GameMusic_SetMenuMusicEnabled(!GameMusic_IsMenuMusicEnabled());
}

#else /* native build without psgplay */

void GameMusic_Init(void) {}
void GameMusic_Shutdown(void) {}
void GameMusic_Pump(void) {}
void GameMusic_OnGameModeChanged(GameModeType prev, GameModeType next) {
    (void)prev;
    (void)next;
}

bool GameMusic_IsMenuMusicEnabled(void) {
    return true;
}

void GameMusic_SetMenuMusicEnabled(bool enabled) {
    (void)enabled;
}

void GameMusic_ToggleMenuMusic(void) {}

#endif /* __EMSCRIPTEN__ */
#endif /* STUNT_PSGPLAY_MUSIC */
