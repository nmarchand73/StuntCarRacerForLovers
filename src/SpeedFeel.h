#ifndef _SPEED_FEEL
#define _SPEED_FEEL

#include "platform_sdl_gl.h"

/** Presentation-only speed impression FX (FOV punch + edge vignette).
 * Driven mainly by forward acceleration / boost surge, not absolute speed.
 * Does not change physics. Toggle with I (P = pause, U = Amiga+ physics).
 */

bool IsSpeedFeelEnabled(void);
void SetSpeedFeelEnabled(bool enabled);
void ToggleSpeedFeel(void);
const char* GetSpeedFeelProfileId(void);

/** Base SCR FOV is PI/4; when enabled, widens with speed/boost. */
float GetSpeedFeelFovY(void);

/** Soft intensity 0..1 for HUD/debug (after smoothing). */
float GetSpeedFeelIntensity(void);

/** Call once per rendered gameplay frame before DrawCockpit. */
void UpdateSpeedFeel(float frameDeltaSeconds);

/** Screen-space vignette over the 3D view (cockpit drawn after stays sharp). */
void DrawSpeedFeelOverlay(RenderDevice* pDevice);

void FreeSpeedFeelResources(void);

#endif /* _SPEED_FEEL */
