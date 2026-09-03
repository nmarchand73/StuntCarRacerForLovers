#ifndef GAME_MUSIC_H
#define GAME_MUSIC_H

#include "platform_sdl_gl.h"
#include "StuntCarRacer.h"

void GameMusic_Init(void);
void GameMusic_Shutdown(void);
void GameMusic_Pump(void);
void GameMusic_OnGameModeChanged(GameModeType prev, GameModeType next);
bool GameMusic_IsEnabled(void);
void GameMusic_SetEnabled(bool enabled);
void GameMusic_Toggle(void);
/* Older names used by menu HUD; same global music mute. */
inline bool GameMusic_IsMenuMusicEnabled(void) {
    return GameMusic_IsEnabled();
}
inline void GameMusic_SetMenuMusicEnabled(bool enabled) {
    GameMusic_SetEnabled(enabled);
}
inline void GameMusic_ToggleMenuMusic(void) {
    GameMusic_Toggle();
}

#if defined(STUNT_PSGPLAY_MUSIC)
void GameMusic_Mix(float* out, int frame_count, int channels);
#endif

#endif /* GAME_MUSIC_H */
