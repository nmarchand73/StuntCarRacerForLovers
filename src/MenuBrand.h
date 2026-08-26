#ifndef MENU_BRAND_H
#define MENU_BRAND_H

#include "platform_sdl_gl.h"

#include <SDL_ttf.h>

void ResetTrackMenuBrandMotion(double timeSeconds);
void ResetTrackPreviewBrandMotion(double timeSeconds);

/** Draw the TRACK_MENU title composition (scrims, wordmark, track CTA, quiet toggles). */
void DrawTrackMenuBrand(TextHelper& bodyText, TTF_Font* displayFont, TTF_Font* scriptFont, GLuint sprite,
                        const wchar_t* trackName, const wchar_t* packName, bool superLeague, bool amigaPhysics,
                        bool speedFeel, bool menuMusic, double timeSeconds);

/** Draw the TRACK_PREVIEW setup composition (track hero, mode, opponent count, CTA). */
void DrawTrackPreviewBrand(TextHelper& bodyText, TTF_Font* displayFont, TTF_Font* scriptFont, GLuint sprite,
                           const wchar_t* trackName, const wchar_t* packName, bool superLeague, bool multiplayer,
                           long opponentCount, double timeSeconds);

#endif
