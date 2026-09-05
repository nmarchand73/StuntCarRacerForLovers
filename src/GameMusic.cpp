#include "platform_sdl_gl.h"
#include "GameMusic.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <vector>

#if defined(STUNT_PSGPLAY_MUSIC)

extern "C" {
#include <psgplay/psgplay.h>
#include <psgplay/stereo.h>
}

#if defined(STUNT_OGG_RACE_MUSIC)
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#endif

#if defined(STUNT_OGG_RACE_MUSIC)
static const char* kPlaylistPaths[] = {
    "data/Music/Playlist/F1.ogg",
    "data/Music/Playlist/Three_Laps_Is_A_Lifetime.ogg",
};
static const int kPlaylistCount = static_cast<int>(sizeof(kPlaylistPaths) / sizeof(kPlaylistPaths[0]));
#else
static const char* kMenuMusicPath = "data/Music/Menu/Blood_Money.sndh";
static const char* kRaceMusicPath = "data/Music/Race/Wings_of_Death_STe.sndh";
static const int kMenuSubtune = 1;
static const int kRaceSubtune = 1;
#endif

static const int kSampleRate = 44100;
static const size_t kPumpChunkFrames = 4096;
static const size_t kTargetQueuedFrames = 22050; /* ~0.5 s */
static const size_t kMaxQueuedFrames = 88200;  /* ~2 s */

enum ActiveMusicEngine {
    MUSIC_ENGINE_NONE = 0,
    MUSIC_ENGINE_PSGPLAY,
    MUSIC_ENGINE_OGG,
};

#if !defined(STUNT_OGG_RACE_MUSIC)
static std::vector<uint8_t> g_menu_sndh;
static std::vector<uint8_t> g_race_sndh;
static const std::vector<uint8_t>* g_active_sndh = NULL;
static int g_active_subtune = 1;
#endif
static struct psgplay* g_psgplay = NULL;
#if defined(STUNT_OGG_RACE_MUSIC)
static stb_vorbis* g_ogg = NULL;
static int g_last_playlist_index = -1;
static const char* g_active_ogg_path = NULL;
#endif
static ActiveMusicEngine g_active_engine = MUSIC_ENGINE_NONE;
static SDL_mutex* g_music_mutex = NULL;
static std::deque<float> g_music_queue;
static const float kMenuMusicGain = 0.40f;
static const float kRaceMusicGain = 0.45f * 0.4f * 1.3f * 1.5f;
static bool g_ready = false;
static bool g_music_enabled = true;
static GameModeType g_last_music_mode = TRACK_MENU;
static float g_music_gain = kMenuMusicGain;

#if !defined(STUNT_OGG_RACE_MUSIC)
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
#endif

static void StopPsgplayLocked() {
    if (g_psgplay) {
        psgplay_free(g_psgplay);
        g_psgplay = NULL;
    }
}

#if defined(STUNT_OGG_RACE_MUSIC)
static void StopOggPlaybackLocked() {
    if (g_ogg) {
        stb_vorbis_close(g_ogg);
        g_ogg = NULL;
    }
    g_active_ogg_path = NULL;
}

static int PickRandomPlaylistIndex() {
    if (kPlaylistCount <= 0) {
        return -1;
    }
    if (kPlaylistCount == 1) {
        return 0;
    }
    int index = static_cast<int>(std::rand() % kPlaylistCount);
    if (index == g_last_playlist_index) {
        index = (index + 1) % kPlaylistCount;
    }
    return index;
}

static bool OpenOggPathLocked(const char* path) {
    StopOggPlaybackLocked();

    int error = 0;
    g_ogg = stb_vorbis_open_filename(path, &error, NULL);
    if (!g_ogg) {
        printf("GameMusic: failed to open OGG %s (stb error %d)\n", path, error);
        return false;
    }

    int16_t probe[4];
    const int probe_frames = stb_vorbis_get_samples_short_interleaved(g_ogg, 2, probe, 4);
    if (probe_frames <= 0) {
        printf("GameMusic: OGG produced no audio samples: %s\n", path);
        StopOggPlaybackLocked();
        return false;
    }
    stb_vorbis_seek_start(g_ogg);
    g_active_ogg_path = path;
    return true;
}

static bool StartRandomOggPlaybackLocked() {
    const int preferred = PickRandomPlaylistIndex();
    if (preferred < 0) {
        return false;
    }

    for (int attempt = 0; attempt < kPlaylistCount; ++attempt) {
        const int index = (preferred + attempt) % kPlaylistCount;
        if (OpenOggPathLocked(kPlaylistPaths[index])) {
            g_last_playlist_index = index;
            printf("GameMusic: playing %s\n", kPlaylistPaths[index]);
            return true;
        }
    }
    return false;
}

static bool PlaylistAvailable() {
    for (int i = 0; i < kPlaylistCount; ++i) {
        FILE* file = fopen(kPlaylistPaths[i], "rb");
        if (file) {
            fclose(file);
            return true;
        }
    }
    return false;
}
#endif

static void StopPlaybackLocked() {
    StopPsgplayLocked();
#if defined(STUNT_OGG_RACE_MUSIC)
    StopOggPlaybackLocked();
#endif
    g_active_engine = MUSIC_ENGINE_NONE;
#if !defined(STUNT_OGG_RACE_MUSIC)
    g_active_sndh = NULL;
#endif
    g_music_queue.clear();
}

static void QueueStereoFloatChunk(float left, float right) {
    while (g_music_queue.size() / 2 >= kMaxQueuedFrames) {
        g_music_queue.pop_front();
        g_music_queue.pop_front();
    }
    g_music_queue.push_back(left);
    g_music_queue.push_back(right);
}

static void QueuePsgplayChunk(const psgplay_stereo* samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        QueueStereoFloatChunk(static_cast<float>(samples[i].left) / 32768.0f,
                              static_cast<float>(samples[i].right) / 32768.0f);
    }
}

static bool StartMenuPlaybackLocked() {
    StopPlaybackLocked();
#if defined(STUNT_OGG_RACE_MUSIC)
    if (!StartRandomOggPlaybackLocked()) {
        return false;
    }
    g_active_engine = MUSIC_ENGINE_OGG;
    return true;
#else
    g_active_sndh = &g_menu_sndh;
    g_active_subtune = kMenuSubtune;
    g_psgplay = psgplay_init(g_menu_sndh.data(), g_menu_sndh.size(), kMenuSubtune, kSampleRate);
    if (!g_psgplay) {
        printf("GameMusic: psgplay_init failed for menu subtune %d\n", kMenuSubtune);
        g_active_sndh = NULL;
        return false;
    }
    g_active_engine = MUSIC_ENGINE_PSGPLAY;
    return true;
#endif
}

static bool StartRacePlaybackLocked() {
    StopPlaybackLocked();
#if defined(STUNT_OGG_RACE_MUSIC)
    if (!StartRandomOggPlaybackLocked()) {
        return false;
    }
    g_active_engine = MUSIC_ENGINE_OGG;
    return true;
#else
    g_active_sndh = &g_race_sndh;
    g_active_subtune = kRaceSubtune;
    g_psgplay = psgplay_init(g_race_sndh.data(), g_race_sndh.size(), kRaceSubtune, kSampleRate);
    if (!g_psgplay) {
        printf("GameMusic: psgplay_init failed for race subtune %d\n", kRaceSubtune);
        g_active_sndh = NULL;
        return false;
    }
    g_active_engine = MUSIC_ENGINE_PSGPLAY;
    return true;
#endif
}

static void RestartCurrentTrackLocked() {
    if (!g_music_enabled) {
        StopPlaybackLocked();
        return;
    }
    if (g_last_music_mode == GAME_IN_PROGRESS) {
        StartRacePlaybackLocked();
    } else {
        StartMenuPlaybackLocked();
    }
}

static void PumpPsgplayLocked() {
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
        QueuePsgplayChunk(chunk, static_cast<size_t>(read_count));
    }
}

#if defined(STUNT_OGG_RACE_MUSIC)
static void PumpOggLocked() {
    if (!g_ogg) {
        return;
    }

    int16_t chunk[kPumpChunkFrames * 2];
    int idle_loops = 0;
    while (g_music_queue.size() / 2 < kTargetQueuedFrames) {
        const int frames = stb_vorbis_get_samples_short_interleaved(g_ogg, 2, chunk, kPumpChunkFrames * 2);
        if (frames > 0) {
            idle_loops = 0;
            for (int i = 0; i < frames; ++i) {
                QueueStereoFloatChunk(static_cast<float>(chunk[i * 2]) / 32768.0f,
                                      static_cast<float>(chunk[i * 2 + 1]) / 32768.0f);
            }
            continue;
        }

        /* End of track: pick another playlist entry at random. */
        if (!StartRandomOggPlaybackLocked()) {
            printf("GameMusic: failed to continue playlist\n");
            StopPlaybackLocked();
            return;
        }
        g_active_engine = MUSIC_ENGINE_OGG;

        ++idle_loops;
        if (idle_loops >= 4) {
            printf("GameMusic: OGG playlist stalled\n");
            StopPlaybackLocked();
            return;
        }
    }
}
#endif

static void PumpLocked() {
    switch (g_active_engine) {
    case MUSIC_ENGINE_PSGPLAY:
        PumpPsgplayLocked();
        break;
#if defined(STUNT_OGG_RACE_MUSIC)
    case MUSIC_ENGINE_OGG:
        PumpOggLocked();
        break;
#endif
    case MUSIC_ENGINE_NONE:
        break;
    default:
        break;
    }
}

static void ApplyMusicForMode(GameModeType mode) {
    g_last_music_mode = mode;

    if (!g_music_enabled) {
        StopPlaybackLocked();
        return;
    }

    if (mode == GAME_IN_PROGRESS) {
        g_music_gain = kRaceMusicGain;
        if (!StartRacePlaybackLocked()) {
            printf("GameMusic: race music unavailable\n");
        }
        return;
    }

    if (mode == TRACK_MENU || mode == TRACK_PREVIEW || mode == GAME_OVER) {
        g_music_gain = kMenuMusicGain;
        if (!StartMenuPlaybackLocked()) {
            printf("GameMusic: menu music unavailable\n");
        }
    }
}

void GameMusic_Init(void) {
    if (g_ready) {
        return;
    }

    std::srand(static_cast<unsigned>(std::time(NULL)) ^ static_cast<unsigned>(SDL_GetTicks()));

#if defined(STUNT_OGG_RACE_MUSIC)
    if (!PlaylistAvailable()) {
        printf("GameMusic: playlist missing under data/Music/Playlist/\n");
        return;
    }
#else
    if (!LoadBinaryFile(kMenuMusicPath, g_menu_sndh)) {
        return;
    }
    if (!LoadBinaryFile(kRaceMusicPath, g_race_sndh)) {
        g_menu_sndh.clear();
        return;
    }
#endif

    g_music_mutex = SDL_CreateMutex();
    if (!g_music_mutex) {
#if !defined(STUNT_OGG_RACE_MUSIC)
        g_menu_sndh.clear();
        g_race_sndh.clear();
#endif
        return;
    }

    g_ready = true;
#if defined(STUNT_OGG_RACE_MUSIC)
    printf("GameMusic: Zimmer playlist ready (F1 / Three Laps, random menu+race)\n");
#else
    printf("GameMusic: loaded menu and race SNDH tracks\n");
#endif
    ApplyMusicForMode(TRACK_MENU);
}

void GameMusic_Shutdown(void) {
    if (!g_ready) {
        return;
    }

    if (g_music_mutex) {
        SDL_LockMutex(g_music_mutex);
        StopPlaybackLocked();
        SDL_UnlockMutex(g_music_mutex);
        SDL_DestroyMutex(g_music_mutex);
        g_music_mutex = NULL;
    }

#if !defined(STUNT_OGG_RACE_MUSIC)
    g_menu_sndh.clear();
    g_race_sndh.clear();
#endif
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

bool GameMusic_IsEnabled(void) {
    return g_music_enabled;
}

void GameMusic_SetEnabled(bool enabled) {
    if (g_music_enabled == enabled) {
        return;
    }

    g_music_enabled = enabled;
    if (!g_ready || !g_music_mutex) {
        return;
    }

    SDL_LockMutex(g_music_mutex);
    if (enabled) {
        ApplyMusicForMode(g_last_music_mode);
        PumpLocked();
    } else {
        StopPlaybackLocked();
    }
    SDL_UnlockMutex(g_music_mutex);
    printf("Music: %s\n", enabled ? "On" : "Off");
}

void GameMusic_Toggle(void) {
    GameMusic_SetEnabled(!g_music_enabled);
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

bool GameMusic_IsEnabled(void) {
    return js_web_music_is_menu_enabled() != 0;
}

void GameMusic_SetEnabled(bool enabled) {
    js_web_music_set_menu_enabled(enabled ? 1 : 0);
}

void GameMusic_Toggle(void) {
    GameMusic_SetEnabled(!GameMusic_IsEnabled());
}

#else /* native build without psgplay */

void GameMusic_Init(void) {}
void GameMusic_Shutdown(void) {}
void GameMusic_Pump(void) {}
void GameMusic_OnGameModeChanged(GameModeType prev, GameModeType next) {
    (void)prev;
    (void)next;
}

bool GameMusic_IsEnabled(void) {
    return true;
}

void GameMusic_SetEnabled(bool enabled) {
    (void)enabled;
}

void GameMusic_Toggle(void) {}

#endif /* __EMSCRIPTEN__ */
#endif /* STUNT_PSGPLAY_MUSIC */
