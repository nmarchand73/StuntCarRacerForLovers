#ifndef _AESTHETICS_FEEL
#define _AESTHETICS_FEEL

#include "platform_sdl_gl.h"

/** Presentation-only Enhanced Look (sky, fog, textures, props, liveries).
 * Does not change physics. Toggle with O (U = Amiga+, I = speed feel; P = pause).
 */

bool IsAestheticsFeelEnabled(void);
void SetAestheticsFeelEnabled(bool enabled);
void ToggleAestheticsFeel(void);
const char* GetAestheticsFeelProfileId(void);

float GetAestheticsFogDensity(void);
float GetAestheticsFogHeightScale(void);
void GetAestheticsFogSkyColor(float* r, float* g, float* b);

/** Classic fog constants when Enhanced is off. */
float GetClassicFogDensity(void);
float GetClassicFogHeightScale(void);
void GetClassicFogSkyColor(float* r, float* g, float* b);

void LoadEnhancedTextures(void);
void FreeEnhancedTextures(void);

GpuTexture* GetEnhancedAsphaltAlbedo(void);
GpuTexture* GetEnhancedAsphaltNormal(void);
GpuTexture* GetEnhancedGroundAlbedo(void);
GpuTexture* GetEnhancedGroundNormal(void);
GpuTexture* GetEnhancedPropsAtlas(void);
GpuTexture* GetEnhancedCarBodyAtlas(void);

/** 0 = player; 1..N = opponents / P2. Classic always returns 0. */
int GetCarLiveryIndex(int slot);
/** Alias for opponents: slot 0..N-1 maps to liveries 1..N when Enhanced. */
int GetOpponentCarLivery(int opponentSlot);

void InvalidateAestheticsDependentBuffers(void);
void NotifyAestheticsChanged(RenderDevice* pDevice);

#endif /* _AESTHETICS_FEEL */
