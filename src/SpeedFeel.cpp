#include "SpeedFeel.h"

#include "Car_Behaviour.h"
#include "StuntCarRacer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern GameModeType GameMode;

#ifndef SCR_SPEED_FEEL_DEFAULT_ON
#define SCR_SPEED_FEEL_DEFAULT_ON 1
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern long player_z_speed;
extern long boost_activated;
extern long engine_z_acceleration;
extern long engine_power;
extern long CalculateDisplaySpeed(void);

/*
 * Speed feel visuals: acceleration-driven FOV + subtle radial blur of the 3D view.
 * Center stays sharp (track readable); blur and soft darkening grow toward the edges.
 * No chromatic fringe / streak overlays — those read as arcade junk on SCR.
 */

static bool g_speedFeelEnabled = (SCR_SPEED_FEEL_DEFAULT_ON != 0);
static float g_speedFeelIntensity = 0.0f;
static float g_speedFeelFovExtra = 0.0f;
static float g_boostFeel = 0.0f;
static long g_prevPlayerZSpeed = 0;
static bool g_havePrevPlayerZSpeed = false;

static GLuint g_captureTex = 0;
static int g_captureW = 0;
static int g_captureH = 0;
static GLuint g_postProgram = 0;
static GLint g_uniTex = -1;
static GLint g_uniAmount = -1;
static GLint g_uniVignette = -1;
static bool g_glReady = false;
static bool g_glFailed = false;

#if !defined(HAVE_GLES) && !defined(__EMSCRIPTEN__)
struct SpeedFeelGL {
    PFNGLCREATESHADERPROC CreateShader;
    PFNGLSHADERSOURCEPROC ShaderSource;
    PFNGLCOMPILESHADERPROC CompileShader;
    PFNGLGETSHADERIVPROC GetShaderiv;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
    PFNGLDELETESHADERPROC DeleteShader;
    PFNGLCREATEPROGRAMPROC CreateProgram;
    PFNGLATTACHSHADERPROC AttachShader;
    PFNGLLINKPROGRAMPROC LinkProgram;
    PFNGLGETPROGRAMIVPROC GetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
    PFNGLDELETEPROGRAMPROC DeleteProgram;
    PFNGLUSEPROGRAMPROC UseProgram;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
    PFNGLUNIFORM1IPROC Uniform1i;
    PFNGLUNIFORM1FPROC Uniform1f;
    PFNGLBINDBUFFERPROC BindBuffer;
    PFNGLBUFFERDATAPROC BufferData;
    PFNGLGENBUFFERSPROC GenBuffers;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;
    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
    PFNGLBINDATTRIBLOCATIONPROC BindAttribLocation;
    PFNGLACTIVETEXTUREPROC ActiveTexture;
};
static SpeedFeelGL gSfGl = {};
static GLuint g_quadVbo = 0;

template <typename T> static bool LoadSfProc(T* out, const char* name) {
    *out = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    return *out != NULL;
}

static bool EnsureSpeedFeelGL(void) {
    if (g_glReady)
        return true;
    if (g_glFailed)
        return false;

    bool ok = true;
    ok &= LoadSfProc(&gSfGl.CreateShader, "glCreateShader");
    ok &= LoadSfProc(&gSfGl.ShaderSource, "glShaderSource");
    ok &= LoadSfProc(&gSfGl.CompileShader, "glCompileShader");
    ok &= LoadSfProc(&gSfGl.GetShaderiv, "glGetShaderiv");
    ok &= LoadSfProc(&gSfGl.GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= LoadSfProc(&gSfGl.DeleteShader, "glDeleteShader");
    ok &= LoadSfProc(&gSfGl.CreateProgram, "glCreateProgram");
    ok &= LoadSfProc(&gSfGl.AttachShader, "glAttachShader");
    ok &= LoadSfProc(&gSfGl.LinkProgram, "glLinkProgram");
    ok &= LoadSfProc(&gSfGl.GetProgramiv, "glGetProgramiv");
    ok &= LoadSfProc(&gSfGl.GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= LoadSfProc(&gSfGl.DeleteProgram, "glDeleteProgram");
    ok &= LoadSfProc(&gSfGl.UseProgram, "glUseProgram");
    ok &= LoadSfProc(&gSfGl.GetUniformLocation, "glGetUniformLocation");
    ok &= LoadSfProc(&gSfGl.Uniform1i, "glUniform1i");
    ok &= LoadSfProc(&gSfGl.Uniform1f, "glUniform1f");
    ok &= LoadSfProc(&gSfGl.BindBuffer, "glBindBuffer");
    ok &= LoadSfProc(&gSfGl.BufferData, "glBufferData");
    ok &= LoadSfProc(&gSfGl.GenBuffers, "glGenBuffers");
    ok &= LoadSfProc(&gSfGl.DeleteBuffers, "glDeleteBuffers");
    ok &= LoadSfProc(&gSfGl.EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= LoadSfProc(&gSfGl.DisableVertexAttribArray, "glDisableVertexAttribArray");
    ok &= LoadSfProc(&gSfGl.VertexAttribPointer, "glVertexAttribPointer");
    ok &= LoadSfProc(&gSfGl.BindAttribLocation, "glBindAttribLocation");
    ok &= LoadSfProc(&gSfGl.ActiveTexture, "glActiveTexture");
    if (!ok) {
        printf("SpeedFeel: OpenGL symbols missing — FOV-only fallback\n");
        g_glFailed = true;
        return false;
    }

    const char* vsSrc =
        "#version 120\n"
        "attribute vec2 aPos;\n"
        "attribute vec2 aUv;\n"
        "varying vec2 vUv;\n"
        "void main() {\n"
        "  vUv = aUv;\n"
        "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    /* Radial blur + soft rim darken. Center stays sharp for gap judgment. */
    const char* fsSrc =
        "#version 120\n"
        "uniform sampler2D uTex;\n"
        "uniform float uAmount;\n"
        "uniform float uVignette;\n"
        "varying vec2 vUv;\n"
        "void main() {\n"
        "  vec2 center = vec2(0.5, 0.47);\n"
        "  vec2 fromC = vUv - center;\n"
        "  float dist = length(fromC * vec2(1.05, 1.0));\n"
        "  float edge = smoothstep(0.12, 0.92, dist);\n"
        "  float blur = clamp(uAmount, 0.0, 1.0) * edge;\n"
        "  vec2 dir = (dist > 1e-4) ? (fromC / dist) : vec2(0.0);\n"
        "  vec2 step = dir * blur * 0.028;\n"
        "  vec3 col = texture2D(uTex, vUv).rgb;\n"
        "  col += texture2D(uTex, vUv - step * 0.4).rgb;\n"
        "  col += texture2D(uTex, vUv - step * 0.8).rgb;\n"
        "  col += texture2D(uTex, vUv - step * 1.25).rgb;\n"
        "  col += texture2D(uTex, vUv + step * 0.4).rgb;\n"
        "  col += texture2D(uTex, vUv + step * 0.8).rgb;\n"
        "  col += texture2D(uTex, vUv + step * 1.25).rgb;\n"
        "  col *= 1.0 / 7.0;\n"
        "  float vig = smoothstep(0.28, 1.05, dist) * clamp(uVignette, 0.0, 1.0);\n"
        "  col *= 1.0 - vig * 0.42;\n"
        "  /* tiny contrast lift in the clear center */\n"
        "  float core = 1.0 - smoothstep(0.0, 0.35, dist);\n"
        "  col = mix(col, col * 1.04, core * uAmount * 0.35);\n"
        "  gl_FragColor = vec4(col, 1.0);\n"
        "}\n";

    GLuint vs = gSfGl.CreateShader(GL_VERTEX_SHADER);
    GLuint fs = gSfGl.CreateShader(GL_FRAGMENT_SHADER);
    gSfGl.ShaderSource(vs, 1, &vsSrc, NULL);
    gSfGl.CompileShader(vs);
    gSfGl.ShaderSource(fs, 1, &fsSrc, NULL);
    gSfGl.CompileShader(fs);

    GLint compiled = GL_FALSE;
    gSfGl.GetShaderiv(vs, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char logBuf[1024] = {0};
        gSfGl.GetShaderInfoLog(vs, 1023, NULL, logBuf);
        printf("SpeedFeel VS compile failed:\n%s\n", logBuf);
        gSfGl.DeleteShader(vs);
        gSfGl.DeleteShader(fs);
        g_glFailed = true;
        return false;
    }
    gSfGl.GetShaderiv(fs, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char logBuf[1024] = {0};
        gSfGl.GetShaderInfoLog(fs, 1023, NULL, logBuf);
        printf("SpeedFeel FS compile failed:\n%s\n", logBuf);
        gSfGl.DeleteShader(vs);
        gSfGl.DeleteShader(fs);
        g_glFailed = true;
        return false;
    }

    g_postProgram = gSfGl.CreateProgram();
    gSfGl.AttachShader(g_postProgram, vs);
    gSfGl.AttachShader(g_postProgram, fs);
    gSfGl.BindAttribLocation(g_postProgram, 0, "aPos");
    gSfGl.BindAttribLocation(g_postProgram, 1, "aUv");
    gSfGl.LinkProgram(g_postProgram);
    gSfGl.DeleteShader(vs);
    gSfGl.DeleteShader(fs);

    GLint linked = GL_FALSE;
    gSfGl.GetProgramiv(g_postProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char logBuf[1024] = {0};
        gSfGl.GetProgramInfoLog(g_postProgram, 1023, NULL, logBuf);
        printf("SpeedFeel link failed:\n%s\n", logBuf);
        gSfGl.DeleteProgram(g_postProgram);
        g_postProgram = 0;
        g_glFailed = true;
        return false;
    }

    g_uniTex = gSfGl.GetUniformLocation(g_postProgram, "uTex");
    g_uniAmount = gSfGl.GetUniformLocation(g_postProgram, "uAmount");
    g_uniVignette = gSfGl.GetUniformLocation(g_postProgram, "uVignette");

    gSfGl.GenBuffers(1, &g_quadVbo);
    /* NDC fullscreen quad: pos.xy, uv.xy */
    const float quad[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f,
    };
    gSfGl.BindBuffer(GL_ARRAY_BUFFER, g_quadVbo);
    gSfGl.BufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    gSfGl.BindBuffer(GL_ARRAY_BUFFER, 0);

    glGenTextures(1, &g_captureTex);
    glBindTexture(GL_TEXTURE_2D, g_captureTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* Allocate immediately so the sampler never sees an incomplete texture. */
    {
        const unsigned char black[4] = {0, 0, 0, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
        g_captureW = 1;
        g_captureH = 1;
    }
    /* Leave complete capture tex bound (avoid texture name 0 on macOS). */

    g_glReady = true;
    return true;
}
#else
static bool EnsureSpeedFeelGL(void) {
    g_glFailed = true;
    return false;
}
#endif

static void InitDefaultFromEnv(void) {
    static bool done = false;
    if (done)
        return;
    done = true;
    const char* env = std::getenv("SCR_SPEED_FEEL");
    if (!env)
        return;
    if (env[0] == '1' && env[1] == '\0')
        g_speedFeelEnabled = true;
    else if (env[0] == '0' && env[1] == '\0')
        g_speedFeelEnabled = false;
}

bool IsSpeedFeelEnabled(void) {
    InitDefaultFromEnv();
    return g_speedFeelEnabled;
}

void SetSpeedFeelEnabled(bool enabled) {
    InitDefaultFromEnv();
    g_speedFeelEnabled = enabled;
    std::printf("Speed feel: %s\n", GetSpeedFeelProfileId());
}

void ToggleSpeedFeel(void) {
    SetSpeedFeelEnabled(!IsSpeedFeelEnabled());
}

const char* GetSpeedFeelProfileId(void) {
    return IsSpeedFeelEnabled() ? "speed-feel-on" : "speed-feel-off";
}

float GetSpeedFeelIntensity(void) {
    return g_speedFeelIntensity;
}

float GetSpeedFeelFovY(void) {
    const float base = static_cast<float>(M_PI) / 4.0f;
    if (!IsSpeedFeelEnabled())
        return base;
    return base + g_speedFeelFovExtra;
}

static float Clamp01(float v) {
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

static float Smooth01(float t) {
    t = Clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

void UpdateSpeedFeel(float frameDeltaSeconds) {
    if (!IsSpeedFeelEnabled()) {
        g_speedFeelIntensity = 0.0f;
        g_speedFeelFovExtra = 0.0f;
        g_boostFeel = 0.0f;
        g_havePrevPlayerZSpeed = false;
        return;
    }

    if (GameMode != GAME_IN_PROGRESS && GameMode != GAME_OVER) {
        g_speedFeelIntensity *= 0.85f;
        g_speedFeelFovExtra *= 0.85f;
        g_boostFeel *= 0.85f;
        g_havePrevPlayerZSpeed = false;
        return;
    }

    const float dt = (frameDeltaSeconds > 0.0f && frameDeltaSeconds < 0.1f) ? frameDeltaSeconds : (1.0f / 60.0f);

    const float maxEngine = static_cast<float>((engine_power > 0) ? (engine_power * 2) : 480);
    const float forwardEngine = (engine_z_acceleration > 0) ? static_cast<float>(engine_z_acceleration) : 0.0f;
    const float accelEngine01 = Clamp01(forwardEngine / maxEngine);

    float accelDelta01 = 0.0f;
    if (g_havePrevPlayerZSpeed) {
        const float dz = static_cast<float>(player_z_speed - g_prevPlayerZSpeed);
        const float surgePerSec = (dt > 1e-4f) ? (dz / dt) : 0.0f;
        accelDelta01 = Clamp01(surgePerSec / 180000.0f);
    }
    g_prevPlayerZSpeed = player_z_speed;
    g_havePrevPlayerZSpeed = true;

    const float boost01 = (boost_activated != 0) ? 1.0f : 0.0f;
    const float speed01 = Clamp01(static_cast<float>(CalculateDisplaySpeed()) / 260.0f);

    const float target = Clamp01(accelEngine01 * 0.55f + accelDelta01 * 0.35f + boost01 * 0.25f + speed01 * 0.12f);
    const float followRate = (target > g_speedFeelIntensity) ? 14.0f : 5.0f;
    const float follow = 1.0f - std::exp(-dt * followRate);
    g_speedFeelIntensity += (target - g_speedFeelIntensity) * follow;

    const float boostFollow = 1.0f - std::exp(-dt * ((boost01 > 0.0f) ? 12.0f : 4.0f));
    g_boostFeel += (boost01 - g_boostFeel) * boostFollow;

    /* Tasteful FOV: max ~5.5° — enough rush, not fisheye. */
    const float maxExtraRad = 5.5f * static_cast<float>(M_PI) / 180.0f;
    const float fovTarget = Smooth01(g_speedFeelIntensity) * maxExtraRad * (1.0f + g_boostFeel * 0.15f);
    g_speedFeelFovExtra += (fovTarget - g_speedFeelFovExtra) * follow;
}

void FreeSpeedFeelResources(void) {
#if !defined(HAVE_GLES) && !defined(__EMSCRIPTEN__)
    if (g_postProgram && gSfGl.DeleteProgram) {
        gSfGl.DeleteProgram(g_postProgram);
        g_postProgram = 0;
    }
    if (g_quadVbo && gSfGl.DeleteBuffers) {
        gSfGl.DeleteBuffers(1, &g_quadVbo);
        g_quadVbo = 0;
    }
#endif
    if (g_captureTex) {
        glDeleteTextures(1, &g_captureTex);
        g_captureTex = 0;
    }
    g_captureW = g_captureH = 0;
    g_glReady = false;
}

void DrawSpeedFeelOverlay(RenderDevice* pDevice) {
    (void)pDevice;
    if (!IsSpeedFeelEnabled())
        return;
    if (g_speedFeelIntensity < 0.02f)
        return;
    if (GameMode != GAME_IN_PROGRESS && GameMode != GAME_OVER)
        return;
#if defined(HAVE_GLES) || defined(__EMSCRIPTEN__)
    return; /* FOV-only on GLES / web for now */
#else
    if (!EnsureSpeedFeelGL())
        return;

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    const int w = vp[2] > 0 ? vp[2] : 640;
    const int h = vp[3] > 0 ? vp[3] : 480;

    GLint prevActive = GL_TEXTURE0;
    GLint prevTex = 0;
    GLint prevProgram = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActive);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

    gSfGl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_captureTex);
    if (w != g_captureW || h != g_captureH) {
        /* RGBA is more reliable than RGB on Apple's GL→Metal path. */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        g_captureW = w;
        g_captureH = h;
    }
    /* Grab the finished 3D view (cockpit drawn after this call). */
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vp[0], vp[1], w, h);

    const float amount = Smooth01(g_speedFeelIntensity) * (0.55f + g_boostFeel * 0.35f);
    const float vignette = Smooth01(g_speedFeelIntensity) * (0.55f + g_boostFeel * 0.25f);

    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWas = glIsEnabled(GL_BLEND);
    GLboolean cullWas = glIsEnabled(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    gSfGl.UseProgram(g_postProgram);
    gSfGl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_captureTex);
    if (g_uniTex >= 0)
        gSfGl.Uniform1i(g_uniTex, 0);
    if (g_uniAmount >= 0)
        gSfGl.Uniform1f(g_uniAmount, amount);
    if (g_uniVignette >= 0)
        gSfGl.Uniform1f(g_uniVignette, vignette);

    gSfGl.BindBuffer(GL_ARRAY_BUFFER, g_quadVbo);
    gSfGl.EnableVertexAttribArray(0);
    gSfGl.EnableVertexAttribArray(1);
    gSfGl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    gSfGl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void*>(sizeof(float) * 2));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    gSfGl.DisableVertexAttribArray(0);
    gSfGl.DisableVertexAttribArray(1);
    gSfGl.BindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTex > 0 ? prevTex : g_captureTex));
    gSfGl.ActiveTexture(static_cast<GLenum>(prevActive));
    gSfGl.UseProgram(static_cast<GLuint>(prevProgram));

    if (depthWas)
        glEnable(GL_DEPTH_TEST);
    if (blendWas)
        glEnable(GL_BLEND);
    if (cullWas)
        glEnable(GL_CULL_FACE);
#endif
}
