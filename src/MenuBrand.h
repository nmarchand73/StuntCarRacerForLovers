#ifndef MENU_BRAND_H
#define MENU_BRAND_H

#include "platform_sdl_gl.h"

#include <SDL_ttf.h>

void ResetTrackMenuBrandMotion(double timeSeconds);
void ResetTrackPreviewBrandMotion(double timeSeconds);

/** Draw the TRACK_MENU title composition (scrims, wordmark, track CTA, quiet toggles).
 *  classicDivision is 1–4 for Classic pack Amiga divisions, or 0 when not applicable. */
void DrawTrackMenuBrand(TextHelper& bodyText, TTF_Font* displayFont, TTF_Font* scriptFont, GLuint sprite,
                        const wchar_t* trackName, const wchar_t* packName, bool superLeague, bool amigaPhysics,
                        bool speedFeel, bool menuMusic, long classicDivision, double timeSeconds);

/** Draw the TRACK_PREVIEW setup composition (track hero, mode, opponent count, CTA). */
void DrawTrackPreviewBrand(TextHelper& bodyText, TTF_Font* displayFont, TTF_Font* scriptFont, GLuint sprite,
                           const wchar_t* trackName, const wchar_t* packName, bool superLeague, bool multiplayer,
                           long opponentCount, long classicDivision, double timeSeconds);

#endif
