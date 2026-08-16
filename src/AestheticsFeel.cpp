#include "AestheticsFeel.h"

#include "Backdrop.h"
#include "Car.h"
#include "Track.h"
#include "TrackProps.h"

#include <cstdlib>
#include <cstring>

#ifndef SCR_AESTHETICS_DEFAULT_ON
#define SCR_AESTHETICS_DEFAULT_ON 1
#endif

static bool g_aestheticsEnabled = (SCR_AESTHETICS_DEFAULT_ON != 0);
static bool g_envInited = false;

static GpuTexture* g_asphaltAlbedo = NULL;
static GpuTexture* g_asphaltNormal = NULL;
static GpuTexture* g_groundAlbedo = NULL;
static GpuTexture* g_groundNormal = NULL;
static GpuTexture* g_propsAtlas = NULL;
static GpuTexture* g_carBodyAtlas = NULL;
static bool g_texturesLoaded = false;

static constexpr float kClassicFogDensity = 0.000008f;
static constexpr float kClassicFogHeightScale = 8.0f;
static constexpr float kClassicFogR = 0.7f;
static constexpr float kClassicFogG = 0.6f;
static constexpr float kClassicFogB = 0.5f;

/* Enhanced stays in SCR's dusty horizon — same fog tint, slightly denser only. */
static constexpr float kEnhancedFogDensity = 0.0000095f;
static constexpr float kEnhancedFogHeightScale = 8.5f;
static constexpr float kEnhancedFogR = 0.7f;
static constexpr float kEnhancedFogG = 0.6f;
static constexpr float kEnhancedFogB = 0.5f;

static void InitDefaultFromEnv(void) {
    if (g_envInited)
        return;
    g_envInited = true;
    const char* env = std::getenv("SCR_AESTHETICS");
    if (env == NULL || env[0] == '\0')
        return;
    if (env[0] == '0' && env[1] == '\0')
        g_aestheticsEnabled = false;
    else if (env[0] == '1' && env[1] == '\0')
        g_aestheticsEnabled = true;
}

bool IsAestheticsFeelEnabled(void) {
    InitDefaultFromEnv();
    return g_aestheticsEnabled;
}

void SetAestheticsFeelEnabled(bool enabled) {
    InitDefaultFromEnv();
    g_aestheticsEnabled = enabled;
}

void ToggleAestheticsFeel(void) {
    InitDefaultFromEnv();
    g_aestheticsEnabled = !g_aestheticsEnabled;
}

const char* GetAestheticsFeelProfileId(void) {
    return IsAestheticsFeelEnabled() ? "enhanced" : "classic";
}

float GetClassicFogDensity(void) { return kClassicFogDensity; }
float GetClassicFogHeightScale(void) { return kClassicFogHeightScale; }
void GetClassicFogSkyColor(float* r, float* g, float* b) {
    if (r)
        *r = kClassicFogR;
    if (g)
        *g = kClassicFogG;
    if (b)
        *b = kClassicFogB;
}

float GetAestheticsFogDensity(void) {
    return IsAestheticsFeelEnabled() ? kEnhancedFogDensity : kClassicFogDensity;
}

float GetAestheticsFogHeightScale(void) {
    return IsAestheticsFeelEnabled() ? kEnhancedFogHeightScale : kClassicFogHeightScale;
}

void GetAestheticsFogSkyColor(float* r, float* g, float* b) {
    if (IsAestheticsFeelEnabled()) {
        if (r)
            *r = kEnhancedFogR;
        if (g)
            *g = kEnhancedFogG;
        if (b)
            *b = kEnhancedFogB;
    } else {
        GetClassicFogSkyColor(r, g, b);
    }
}

void LoadEnhancedTextures(void) {
    if (g_texturesLoaded)
        return;

    g_asphaltAlbedo = new GpuTexture();
    g_asphaltNormal = new GpuTexture();
    g_groundAlbedo = new GpuTexture();
    g_groundNormal = new GpuTexture();
    g_propsAtlas = new GpuTexture();
    g_carBodyAtlas = new GpuTexture();

    g_asphaltAlbedo->LoadTexture("data/Bitmap/enhanced/asphalt_albedo.png");
    g_asphaltNormal->LoadTexture("data/Bitmap/enhanced/asphalt_normal.png");
    g_groundAlbedo->LoadTexture("data/Bitmap/enhanced/ground_albedo.png");
    g_groundNormal->LoadTexture("data/Bitmap/enhanced/ground_normal.png");
    g_propsAtlas->LoadTexture("data/Bitmap/enhanced/props_atlas.png");
    g_carBodyAtlas->LoadTexture("data/Bitmap/enhanced/car_body_atlas.png");

    g_texturesLoaded = true;
}

void FreeEnhancedTextures(void) {
    delete g_asphaltAlbedo;
    delete g_asphaltNormal;
    delete g_groundAlbedo;
    delete g_groundNormal;
    delete g_propsAtlas;
    delete g_carBodyAtlas;
    g_asphaltAlbedo = g_asphaltNormal = g_groundAlbedo = g_groundNormal = NULL;
    g_propsAtlas = g_carBodyAtlas = NULL;
    g_texturesLoaded = false;
}

GpuTexture* GetEnhancedAsphaltAlbedo(void) { return g_asphaltAlbedo; }
GpuTexture* GetEnhancedAsphaltNormal(void) { return g_asphaltNormal; }
GpuTexture* GetEnhancedGroundAlbedo(void) { return g_groundAlbedo; }
GpuTexture* GetEnhancedGroundNormal(void) { return g_groundNormal; }
GpuTexture* GetEnhancedPropsAtlas(void) { return g_propsAtlas; }
GpuTexture* GetEnhancedCarBodyAtlas(void) { return g_carBodyAtlas; }

int GetCarLiveryIndex(int slot) {
    if (!IsAestheticsFeelEnabled())
        return 0;
    if (slot < 0)
        return 0;
    /* 0 player, 1..4 opponents / P2 — clamp to atlas strips */
    if (slot > 4)
        return 4;
    return slot;
}

int GetOpponentCarLivery(int opponentSlot) {
    return GetCarLiveryIndex(opponentSlot + 1);
}

void InvalidateAestheticsDependentBuffers(void) {
    InvalidateBackdropAesthetics();
    FreeGroundPlaneVertexBuffer();
    FreeTrackPropsVertexBuffer();
    FreeCarVertexBuffer();
    FreeAsphaltRoadVertexBuffer();
    FreeTrackVertexBuffer();
}

void NotifyAestheticsChanged(RenderDevice* pDevice) {
    InvalidateAestheticsDependentBuffers();
    if (pDevice == NULL)
        return;
    if (IsAestheticsFeelEnabled())
        LoadEnhancedTextures();
    CreateGroundPlaneVertexBuffer(pDevice);
    CreateTrackVertexBuffer(pDevice);
    CreateCarVertexBuffer(pDevice);
    RebuildTrackProps(pDevice);
    RebuildAsphaltRoadVertexBuffer(pDevice);
}
