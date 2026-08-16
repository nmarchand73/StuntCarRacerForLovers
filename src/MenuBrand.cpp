#include "MenuBrand.h"

#include "StuntCarRacer.h"

#include <cmath>
#include <cstdio>
#include <sstream>

/*
 * Stunt Car Racer for Lovers — menu brand language
 *   --ink      deep night scrims (slight warm bias, not purple)
 *   --paper    bright wordmark
 *   --rose     "for Lovers" + soft brand rule
 *   --amber    race CTA
 * Dual type: condensed racing display + script intimacy. Live track stays the hero image.
 */

namespace {

struct MenuColVert {
    float x, y, z, rhw;
    DWORD color;
};

VertexBuffer* g_menuVb = NULL;
double g_menuEnterTime = -1.0;
double g_previewEnterTime = -1.0;

int MenuProjectionWidth(void) {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[3] <= 0)
        return (vp[2] > 0) ? vp[2] : BASE_WIDTH_STANDARD;
    return static_cast<int>((480.0f * static_cast<float>(vp[2]) / static_cast<float>(vp[3])) + 0.5f);
}

float Clampf(float v, float lo, float hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

float Smooth01(float t) {
    t = Clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

bool EnsureMenuVb(void) {
    if (g_menuVb)
        return true;
    RenderDevice* dev = GetRenderDevice();
    if (FAILED(dev->CreateVertexBuffer(8 * sizeof(MenuColVert), VB_USAGE_WRITEONLY, FVF_XYZRHW | FVF_DIFFUSE,
                                       POOL_DEFAULT, &g_menuVb, NULL))) {
        printf("MenuBrand: failed to create overlay VB\n");
        return false;
    }
    return true;
}

void DrawGradientRect(float x1, float y1, float x2, float y2, DWORD cTop, DWORD cBottom) {
    if (!EnsureMenuVb())
        return;

    MenuColVert* v = NULL;
    if (FAILED(g_menuVb->Lock(0, 0, (void**)&v, 0)))
        return;

    const float z = 0.1f;
    const float rhw = 1.0f;
    v[0].x = x1;
    v[0].y = y1;
    v[0].z = z;
    v[0].rhw = rhw;
    v[0].color = cTop;
    v[1].x = x2;
    v[1].y = y1;
    v[1].z = z;
    v[1].rhw = rhw;
    v[1].color = cTop;
    v[2].x = x2;
    v[2].y = y2;
    v[2].z = z;
    v[2].rhw = rhw;
    v[2].color = cBottom;
    v[3].x = x1;
    v[3].y = y2;
    v[3].z = z;
    v[3].rhw = rhw;
    v[3].color = cBottom;
    g_menuVb->Unlock();

    RenderDevice* dev = GetRenderDevice();
    dev->SetRenderState(RS_ZENABLE, FALSE);
    dev->SetRenderState(RS_CULLMODE, CULL_NONE);
    dev->SetRenderState(RS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(RS_SRCBLEND, BLEND_SRCALPHA);
    dev->SetRenderState(RS_DESTBLEND, BLEND_INVSRCALPHA);
    dev->SetTextureStageState(0, TSS_COLOROP, TOP_DISABLE);
    dev->SetTexture(0, NULL);
    dev->SetStreamSource(0, g_menuVb, 0, sizeof(MenuColVert));
    dev->SetFVF(FVF_XYZRHW | FVF_DIFFUSE);
    dev->DrawPrimitive(PT_TRIANGLEFAN, 0, 2);
    dev->SetRenderState(RS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(RS_ZENABLE, TRUE);
}

void DrawSolidRect(float x1, float y1, float x2, float y2, DWORD color) {
    DrawGradientRect(x1, y1, x2, y2, color, color);
}

/** Horizontal fade (left/right colors) — for elegant brand rules. */
void DrawHorizontalGradientRect(float x1, float y1, float x2, float y2, DWORD cLeft, DWORD cRight) {
    if (!EnsureMenuVb())
        return;

    MenuColVert* v = NULL;
    if (FAILED(g_menuVb->Lock(0, 0, (void**)&v, 0)))
        return;

    const float z = 0.1f;
    const float rhw = 1.0f;
    v[0].x = x1;
    v[0].y = y1;
    v[0].z = z;
    v[0].rhw = rhw;
    v[0].color = cLeft;
    v[1].x = x2;
    v[1].y = y1;
    v[1].z = z;
    v[1].rhw = rhw;
    v[1].color = cRight;
    v[2].x = x2;
    v[2].y = y2;
    v[2].z = z;
    v[2].rhw = rhw;
    v[2].color = cRight;
    v[3].x = x1;
    v[3].y = y2;
    v[3].z = z;
    v[3].rhw = rhw;
    v[3].color = cLeft;
    g_menuVb->Unlock();

    RenderDevice* dev = GetRenderDevice();
    dev->SetRenderState(RS_ZENABLE, FALSE);
    dev->SetRenderState(RS_CULLMODE, CULL_NONE);
    dev->SetRenderState(RS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(RS_SRCBLEND, BLEND_SRCALPHA);
    dev->SetRenderState(RS_DESTBLEND, BLEND_INVSRCALPHA);
    dev->SetTextureStageState(0, TSS_COLOROP, TOP_DISABLE);
    dev->SetTexture(0, NULL);
    dev->SetStreamSource(0, g_menuVb, 0, sizeof(MenuColVert));
    dev->SetFVF(FVF_XYZRHW | FVF_DIFFUSE);
    dev->DrawPrimitive(PT_TRIANGLEFAN, 0, 2);
    dev->SetRenderState(RS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(RS_ZENABLE, TRUE);
}

/** Centered brand rule: edge-faded hairlines + soft rose bloom (no curb checkers). */
void DrawBrandRule(float midX, float y, float halfWidth, float alpha) {
    const BYTE a = static_cast<BYTE>(Clampf(alpha, 0.0f, 1.0f) * 255.0f);
    const DWORD clear = RGBA_MAKE(255, 255, 255, 0);
    const DWORD paper = RGBA_MAKE(245, 238, 232, a);
    const DWORD roseCore = RGBA_MAKE(232, 130, 150, static_cast<BYTE>(a * 0.85f));
    const DWORD roseSoft = RGBA_MAKE(232, 130, 150, 0);

    const float x0 = midX - halfWidth;
    const float x1 = midX;
    const float x2 = midX + halfWidth;

    /* Soft rose bloom under the rule */
    DrawHorizontalGradientRect(x0, y - 1.5f, x1, y + 3.5f, roseSoft, roseCore);
    DrawHorizontalGradientRect(x1, y - 1.5f, x2, y + 3.5f, roseCore, roseSoft);

    /* Twin hairlines fading at the ends */
    DrawHorizontalGradientRect(x0, y, x1, y + 1.0f, clear, paper);
    DrawHorizontalGradientRect(x1, y, x2, y + 1.0f, paper, clear);
    DrawHorizontalGradientRect(x0, y + 3.0f, x1, y + 4.0f, clear, paper);
    DrawHorizontalGradientRect(x1, y + 3.0f, x2, y + 4.0f, paper, clear);

    /* Small center diamond */
    const float d = 3.2f;
    const DWORD rose = RGBA_MAKE(236, 150, 165, a);
    DrawSolidRect(midX - d, y + 0.5f, midX + d, y + 3.5f, rose);
    DrawSolidRect(midX - 1.2f, y - 1.0f, midX + 1.2f, y + 5.0f, rose);
}

void DrawCentered(TextHelper& txt, const std::wstring& line, int y) {
    int x = (MenuProjectionWidth() - txt.MeasureTextWidth(line.c_str())) / 2;
    if (x < 0)
        x = 0;
    txt.SetInsertionPos(x, y);
    txt.DrawFormattedTextLine(line);
}

void DrawCenteredShadow(TextHelper& txt, const std::wstring& line, int y, float r, float g, float b, float a,
                        int shadowPx) {
    txt.SetForegroundColor(glm::vec4(0.05f, 0.02f, 0.04f, Clampf(a * 0.7f, 0.0f, 1.0f)));
    DrawCentered(txt, line, y + shadowPx);
    txt.SetForegroundColor(glm::vec4(r, g, b, a));
    DrawCentered(txt, line, y);
}

TextHelper& BindFontHelper(TextHelper*& slot, TTF_Font*& bound, TTF_Font* font, GLuint sprite, int size,
                           TextHelper& fallback) {
    if (font && (slot == NULL || bound != font)) {
        delete slot;
        slot = new TextHelper(font, sprite, size);
        bound = font;
    }
    return slot ? *slot : fallback;
}

} // namespace

void ResetTrackMenuBrandMotion(double timeSeconds) {
    g_menuEnterTime = timeSeconds;
}

void ResetTrackPreviewBrandMotion(double timeSeconds) {
    g_previewEnterTime = timeSeconds;
}

void DrawTrackMenuBrand(TextHelper& bodyText, TTF_Font* displayFont, TTF_Font* scriptFont, GLuint sprite,
                        const wchar_t* trackName, const wchar_t* packName, bool superLeague, bool amigaPhysics,
                        bool speedFeel, double timeSeconds) {
    const float W = static_cast<float>(MenuProjectionWidth());
    const float H = 480.0f;

    if (g_menuEnterTime < 0.0)
        g_menuEnterTime = timeSeconds;

    const float enterT = Smooth01(static_cast<float>((timeSeconds - g_menuEnterTime) / 0.95));
    const float brandYOffset = (1.0f - enterT) * 22.0f;
    const float brandAlpha = enterT;
    const float scriptDelay = Smooth01(static_cast<float>((timeSeconds - g_menuEnterTime - 0.18) / 0.7));
    const float pulse = 0.58f + 0.42f * (0.5f + 0.5f * std::sin(static_cast<float>(timeSeconds) * 2.2f));
    const float linePulse = 0.75f + 0.25f * (0.5f + 0.5f * std::sin(static_cast<float>(timeSeconds) * 1.35f));
    const float heartBob = 2.0f * std::sin(static_cast<float>(timeSeconds) * 1.8f);

    /* Warm night scrims — leave the track vista open in the middle */
    DrawGradientRect(0.0f, 0.0f, W, 178.0f, RGBA_MAKE(12, 8, 14, 220), RGBA_MAKE(18, 10, 16, 0));
    DrawGradientRect(0.0f, H - 158.0f, W, H, RGBA_MAKE(18, 10, 16, 0), RGBA_MAKE(10, 6, 12, 235));
    /* Soft side falloff so the wordmark sits in a pocket without boxing the track */
    DrawGradientRect(0.0f, 0.0f, W * 0.14f, H, RGBA_MAKE(10, 6, 12, 90), RGBA_MAKE(10, 6, 12, 0));
    DrawGradientRect(W * 0.86f, 0.0f, W, H, RGBA_MAKE(10, 6, 12, 0), RGBA_MAKE(10, 6, 12, 90));

    /* Elegant brand rule under the wordmark — edge-faded hairlines + soft rose bloom */
    {
        const float barY = 118.0f + brandYOffset * 0.3f;
        const float mid = W * 0.5f;
        const float half = (110.0f + 28.0f * linePulse) * brandAlpha;
        DrawBrandRule(mid, barY, half, brandAlpha * (0.75f + 0.25f * linePulse));
    }

    static TextHelper* s_title = NULL;
    static TTF_Font* s_titleFont = NULL;
    static TextHelper* s_script = NULL;
    static TTF_Font* s_scriptFont = NULL;

    TextHelper& title = BindFontHelper(s_title, s_titleFont, displayFont, sprite, 52, bodyText);
    TextHelper& script = BindFontHelper(s_script, s_scriptFont, scriptFont, sprite, 48, bodyText);

    /* Hero: racing condensed + intimate script */
    title.SetDisplaySize(46);
    DrawCenteredShadow(title, L"STUNT CAR RACER", static_cast<int>(22.0f + brandYOffset), 0.97f, 0.95f, 0.92f,
                       brandAlpha, 2);

    script.SetDisplaySize(44);
    DrawCenteredShadow(script, L"for Lovers", static_cast<int>(62.0f + brandYOffset + heartBob * 0.35f), 0.95f, 0.62f,
                       0.72f, brandAlpha * scriptDelay, 2);

    bodyText.SetDisplaySize(13);
    DrawCenteredShadow(bodyText, L"The classic jump. Shared.", static_cast<int>(108.0f + brandYOffset * 0.4f), 0.82f,
                       0.74f, 0.76f, brandAlpha * 0.9f, 1);

    const wchar_t* safeTrack = trackName ? trackName : L"None";
    const wchar_t* safePack = packName ? packName : L"Classic";

    title.SetDisplaySize(36);
    DrawCenteredShadow(title, safeTrack, 286, 0.99f, 0.97f, 0.95f, 0.98f, 2);

    bodyText.SetDisplaySize(13);
    {
        std::wstringstream ss;
        ss << safePack << L" pack";
        if (superLeague)
            ss << L"  |  Super League";
        DrawCenteredShadow(bodyText, ss.str(), 322, 0.78f, 0.68f, 0.70f, 0.95f, 1);
    }

    title.SetDisplaySize(24);
    DrawCenteredShadow(title, L"ENTER  -  RACE", 388, 0.98f, 0.78f, 0.35f, 0.55f + 0.45f * pulse, 1);

    bodyText.SetDisplaySize(12);
    DrawCenteredShadow(bodyText, L"<  >  change track", 416, 0.72f, 0.68f, 0.70f, 0.9f, 1);

    bodyText.SetDisplaySize(11);
    {
        std::wstringstream ss;
        ss << L"Physics " << (amigaPhysics ? L"Amiga+" : L"Classic") << L" [U]  |  Speed "
           << (speedFeel ? L"On" : L"Off") << L" [I]  |  League [L]";
        DrawCenteredShadow(bodyText, ss.str(), 446, 0.58f, 0.52f, 0.54f, 0.82f, 1);
    }
}

void DrawTrackPreviewBrand(TextHelper& bodyText, TTF_Font* displayFont, TTF_Font* scriptFont, GLuint sprite,
                           const wchar_t* trackName, const wchar_t* packName, bool superLeague, bool multiplayer,
                           long opponentCount, double timeSeconds) {
    const float W = static_cast<float>(MenuProjectionWidth());
    const float H = 480.0f;

    if (g_previewEnterTime < 0.0)
        g_previewEnterTime = timeSeconds;

    const float enterT = Smooth01(static_cast<float>((timeSeconds - g_previewEnterTime) / 0.75));
    const float brandAlpha = enterT;
    const float setupAlpha = Smooth01(static_cast<float>((timeSeconds - g_previewEnterTime - 0.12) / 0.65));
    const float pulse = 0.58f + 0.42f * (0.5f + 0.5f * std::sin(static_cast<float>(timeSeconds) * 2.2f));
    const float linePulse = 0.75f + 0.25f * (0.5f + 0.5f * std::sin(static_cast<float>(timeSeconds) * 1.35f));

    DrawGradientRect(0.0f, 0.0f, W, 150.0f, RGBA_MAKE(12, 8, 14, 215), RGBA_MAKE(18, 10, 16, 0));
    DrawGradientRect(0.0f, H - 170.0f, W, H, RGBA_MAKE(18, 10, 16, 0), RGBA_MAKE(10, 6, 12, 235));
    DrawGradientRect(0.0f, 0.0f, W * 0.12f, H, RGBA_MAKE(10, 6, 12, 70), RGBA_MAKE(10, 6, 12, 0));
    DrawGradientRect(W * 0.88f, 0.0f, W, H, RGBA_MAKE(10, 6, 12, 0), RGBA_MAKE(10, 6, 12, 70));

    {
        const float mid = W * 0.5f;
        const float half = (100.0f + 24.0f * linePulse) * brandAlpha;
        DrawBrandRule(mid, 96.0f, half, brandAlpha * (0.7f + 0.3f * linePulse));
    }

    static TextHelper* s_title = NULL;
    static TTF_Font* s_titleFont = NULL;
    static TextHelper* s_script = NULL;
    static TTF_Font* s_scriptFont = NULL;
    TextHelper& title = BindFontHelper(s_title, s_titleFont, displayFont, sprite, 52, bodyText);
    TextHelper& script = BindFontHelper(s_script, s_scriptFont, scriptFont, sprite, 36, bodyText);

    script.SetDisplaySize(28);
    DrawCenteredShadow(script, L"for Lovers", 14, 0.92f, 0.60f, 0.70f, brandAlpha * 0.85f, 1);

    const wchar_t* safeTrack = trackName ? trackName : L"None";
    const wchar_t* safePack = packName ? packName : L"Classic";

    title.SetDisplaySize(42);
    DrawCenteredShadow(title, safeTrack, 42, 0.98f, 0.96f, 0.93f, brandAlpha, 2);

    bodyText.SetDisplaySize(13);
    {
        std::wstringstream ss;
        ss << safePack << L" pack";
        if (superLeague)
            ss << L"  |  Super League";
        DrawCenteredShadow(bodyText, ss.str(), 84, 0.78f, 0.70f, 0.72f, brandAlpha * 0.95f, 1);
    }

    /* Setup rows */
    bodyText.SetDisplaySize(15);
    {
        std::wstringstream ss;
        ss << L"<  Mode   " << (multiplayer ? L"Multiplayer" : L"Single Player") << L"  >";
        DrawCenteredShadow(bodyText, ss.str(), 268, 0.96f, 0.94f, 0.90f, setupAlpha, 1);
    }

    if (!multiplayer) {
        long n = opponentCount;
        if (n < 1)
            n = 1;
        if (n > 4)
            n = 4;
        bodyText.SetDisplaySize(15);
        {
            std::wstringstream ss;
            ss << L"^  Opponents   " << n << L"  v";
            DrawCenteredShadow(bodyText, ss.str(), 292, 0.95f, 0.78f, 0.82f, setupAlpha, 1);
        }
        bodyText.SetDisplaySize(12);
        DrawCenteredShadow(bodyText, L"Up / Down to change pack size", 314, 0.65f, 0.60f, 0.62f, setupAlpha * 0.9f, 1);
    } else {
        bodyText.SetDisplaySize(12);
        DrawCenteredShadow(bodyText, L"Left / Right to change mode", 292, 0.65f, 0.60f, 0.62f, setupAlpha * 0.9f, 1);
    }

    title.SetDisplaySize(24);
    DrawCenteredShadow(title, L"ENTER  -  RACE", 372, 0.98f, 0.78f, 0.35f, 0.55f + 0.45f * pulse, 1);

    bodyText.SetDisplaySize(12);
    DrawCenteredShadow(bodyText, L"M / Esc back   |   F4 scenery", 400, 0.70f, 0.66f, 0.68f, 0.9f, 1);

    bodyText.SetDisplaySize(11);
    DrawCenteredShadow(bodyText, L"Steer  Accel  Brake  Boost  |  P pause  R reverse", 430, 0.55f, 0.52f, 0.54f, 0.82f,
                       1);
}

