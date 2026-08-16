/**************************************************************************
    TrackProps.cpp - Enhanced Look roadside props + living ambient
 **************************************************************************/

#include "TrackProps.h"

#include "AestheticsFeel.h"
#include "StuntCarRacer.h"
#include "Track.h"
#include "3D_Engine.h"

#include <cmath>

extern TRACK_PIECE Track[MAX_PIECES_PER_TRACK];
extern long NumTrackPieces;
extern long TrackID;
extern DWORD SCRGB(long colour_index);

#define SCR_BASE_COLOUR 26
#define MAX_PROP_VERTICES 48000
#define MAX_LIFE_VERTICES 48000
#define MAX_CUBE_FIELD_VERTICES 120000
#define ASPHALT_UV_SCALE 0.0018f

static VertexBuffer* pPropsVB = NULL;
static long numPropVertices = 0;

static VertexBuffer* pCubeFieldVB = NULL;
static long numCubeFieldVertices = 0;

static VertexBuffer* pLifeVB = NULL;
static long numLifeVertices = 0;
static float g_lifeStamp = -1.0f;

enum { kMaxLifeCarTargets = 8, kChaseDroneCount = 2 };

struct LifeCarTarget {
    float x, y, z;
    float yaw;
};

struct ChaseDrone {
    float x, y, z;
    float yaw;
    float rotor;
    int targetIndex;
    float sideSign;
    float hoverBias;
    float leadBias;
    bool alive;
};

static LifeCarTarget g_carTargets[kMaxLifeCarTargets];
static int g_carTargetCount = 0;
static ChaseDrone g_drones[kChaseDroneCount];
static bool g_dronesBooted = false;
static float g_dronePrevTime = -1.0f;

void ClearTrackLifeCarTargets(void) {
    g_carTargetCount = 0;
}

void AddTrackLifeCarTarget(float worldX, float worldY, float worldZ, float yawRadians) {
    if (g_carTargetCount >= kMaxLifeCarTargets)
        return;
    LifeCarTarget& t = g_carTargets[g_carTargetCount++];
    t.x = worldX;
    t.y = worldY;
    t.z = worldZ;
    t.yaw = yawRadians;
}

static void ResetChaseDrones(void) {
    g_dronesBooted = false;
    g_dronePrevTime = -1.0f;
    for (int i = 0; i < kChaseDroneCount; ++i) {
        g_drones[i].alive = false;
        g_drones[i].targetIndex = -1;
    }
}

static VertexBuffer* pAsphaltVB = NULL;
static long numAsphaltVertices = 0;
static long numAsphaltRoadTris = 0;

static unsigned PropHash(long piece, long s, long salt) {
    return (unsigned)(piece * 73856093u) ^ (unsigned)(s * 19349663u) ^ (unsigned)(salt * 83492791u);
}

static void EmitTri(UTVERTEX* verts, long* count, long maxVerts, const glm::vec3& a, const glm::vec3& b,
                    const glm::vec3& c, DWORD colour) {
    if (verts == NULL || count == NULL || (*count) + 3 > maxVerts)
        return;
    verts[*count].pos = a;
    verts[*count].color = colour;
    verts[*count].tu = 0.0f;
    verts[*count].tv = 0.0f;
    ++(*count);
    verts[*count].pos = b;
    verts[*count].color = colour;
    verts[*count].tu = 0.0f;
    verts[*count].tv = 0.0f;
    ++(*count);
    verts[*count].pos = c;
    verts[*count].color = colour;
    verts[*count].tu = 0.0f;
    verts[*count].tv = 0.0f;
    ++(*count);
}

static void EmitQuad(UTVERTEX* verts, long* count, long maxVerts, const glm::vec3& a, const glm::vec3& b,
                     const glm::vec3& c, const glm::vec3& d, DWORD colour) {
    EmitTri(verts, count, maxVerts, a, b, c, colour);
    EmitTri(verts, count, maxVerts, a, c, d, colour);
}

static void EmitBox(UTVERTEX* verts, long* count, long maxVerts, const glm::vec3& c, const glm::vec3& along,
                    const glm::vec3& side, float halfAlong, float halfSide, float height, DWORD colour) {
    const glm::vec3 a = along * halfAlong;
    const glm::vec3 s = side * halfSide;
    const glm::vec3 up(0, height, 0);
    const glm::vec3 b0 = c - a - s;
    const glm::vec3 b1 = c + a - s;
    const glm::vec3 b2 = c + a + s;
    const glm::vec3 b3 = c - a + s;
    EmitQuad(verts, count, maxVerts, b0, b1, b1 + up, b0 + up, colour);
    EmitQuad(verts, count, maxVerts, b1, b2, b2 + up, b1 + up, colour);
    EmitQuad(verts, count, maxVerts, b2, b3, b3 + up, b2 + up, colour);
    EmitQuad(verts, count, maxVerts, b3, b0, b0 + up, b3 + up, colour);
    EmitQuad(verts, count, maxVerts, b0 + up, b1 + up, b2 + up, b3 + up, colour);
}

static glm::vec3 PieceWorldVertex(long piece, long offset);
static float WorldCenter(void);

static void EmitAABox(UTVERTEX* verts, long* count, long maxVerts, float x, float y, float z, float hx, float hz,
                      float height, DWORD colour) {
    EmitBox(verts, count, maxVerts, glm::vec3(x, y, z), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), hx, hz, height,
            colour);
}

static bool NearAnyTrackPiece(float x, float z, float minDist) {
    const float min2 = minDist * minDist;
    for (long piece = 0; piece < NumTrackPieces; ++piece) {
        if (Track[piece].coords == NULL)
            continue;
        const long segs = Track[piece].numSegments;
        if (segs < 1)
            continue;
        const long mid = (segs / 2) * 4;
        const glm::vec3 p = PieceWorldVertex(piece, mid);
        const float dx = p.x - x;
        const float dz = p.z - z;
        if (dx * dx + dz * dz < min2)
            return true;
        const glm::vec3 p2 = PieceWorldVertex(piece, mid + 1);
        const float mx = 0.5f * (p.x + p2.x) - x;
        const float mz = 0.5f * (p.z + p2.z) - z;
        if (mx * mx + mz * mz < min2)
            return true;
    }
    return false;
}

/** Minecraft-inspired SCR cube field: real voxel stacks (not flat stickers). */
static void RebuildCubeField(RenderDevice* pDevice, float groundY) {
    if (pCubeFieldVB) {
        pCubeFieldVB->Release();
        pCubeFieldVB = NULL;
    }
    numCubeFieldVertices = 0;
    if (pDevice == NULL)
        return;

    if (FAILED(pDevice->CreateVertexBuffer(MAX_CUBE_FIELD_VERTICES * sizeof(UTVERTEX), VB_USAGE_WRITEONLY,
                                           FVF_UTVERTEX, POOL_DEFAULT, &pCubeFieldVB, NULL)))
        return;

    UTVERTEX* pVertices = NULL;
    if (FAILED(pCubeFieldVB->Lock(0, 0, (void**)&pVertices, 0))) {
        pCubeFieldVB->Release();
        pCubeFieldVB = NULL;
        return;
    }

    /* Cube field SCR materials (palette indices relative to SCR_BASE_COLOUR):
     * 0 black, 1 khaki, 2 cream, 3 yellow, 4 lime, 5 teal, 6 cyan, 7 sky,
     * 10 brown, 12 rose, 13 olive, 14 grey, 15 white, 17 soft green, 22 orange. */
    const DWORD dirt = SCRGB(SCR_BASE_COLOUR + 10);     /* brown */
    const DWORD grass = SCRGB(SCR_BASE_COLOUR + 13);    /* olive */
    const DWORD moss = SCRGB(SCR_BASE_COLOUR + 4);      /* lime */
    const DWORD wood = SCRGB(SCR_BASE_COLOUR + 10);     /* brown trunk (not hot orange) */
    const DWORD leaf = SCRGB(SCR_BASE_COLOUR + 4);      /* lime canopy */
    const DWORD leafLit = SCRGB(SCR_BASE_COLOUR + 17);  /* soft green highlight */
    const DWORD stone = SCRGB(SCR_BASE_COLOUR + 14);    /* grey */
    const DWORD dark = SCRGB(SCR_BASE_COLOUR + 0);      /* black */
    const DWORD sand = SCRGB(SCR_BASE_COLOUR + 2);      /* cream (not pure yellow) */
    const DWORD water = SCRGB(SCR_BASE_COLOUR + 6);     /* cyan (was brick +11) */
    const DWORD clay = SCRGB(SCR_BASE_COLOUR + 1);      /* khaki mesa */

    const float cx = WorldCenter();
    const float cz = WorldCenter();
    /* One Minecraft-like block ≈ cube on the arena grid. */
    const float B = 96.0f;
    const float cell = B * 2.4f;
    const int half = 80;
    /* Cubes belong outside the circuit — never in the infield.
     * Measure track outer radius from piece midpoints, then start beyond that. */
    float trackOuter = 0.0f;
    for (long piece = 0; piece < NumTrackPieces; ++piece) {
        if (Track[piece].coords == NULL)
            continue;
        const long segs = Track[piece].numSegments;
        if (segs < 1)
            continue;
        const long mid = (segs / 2) * 4;
        const glm::vec3 p = PieceWorldVertex(piece, mid);
        const float dx = p.x - cx;
        const float dz = p.z - cz;
        const float rr = std::sqrt(dx * dx + dz * dz);
        if (rr > trackOuter)
            trackOuter = rr;
    }
    const float rMin = trackOuter + 900.0f;
    const float rMax = trackOuter + 12000.0f;

    auto putBlock = [&](float bx, float by, float bz, DWORD col) {
        EmitAABox(pVertices, &numCubeFieldVertices, MAX_CUBE_FIELD_VERTICES, bx, by, bz, B * 0.48f, B * 0.48f, B,
                  col);
    };

    for (int iz = -half; iz <= half; ++iz) {
        for (int ix = -half; ix <= half; ++ix) {
            if (numCubeFieldVertices + 240 > MAX_CUBE_FIELD_VERTICES)
                goto done;

            const float x = cx + (float)ix * cell;
            const float z = cz + (float)iz * cell;
            const float dx = x - cx;
            const float dz = z - cz;
            const float r2 = dx * dx + dz * dz;
            if (r2 < rMin * rMin || r2 > rMax * rMax)
                continue;
            /* Extra clearance from rails so roadside props own the near band. */
            if (NearAnyTrackPiece(x, z, 720.0f))
                continue;

            const unsigned h = PropHash(ix + 4096, iz + 4096, 777);
            const float ang = std::atan2(dz, dx);
            const float rr = std::sqrt(r2);
            const float bio =
                0.5f + 0.5f * std::sin(ang * 2.0f + rr * 0.00015f) * std::cos(ang * 0.7f - rr * 0.00011f);
            int biome = 0;
            if (bio > 0.62f)
                biome = 1;
            else if (bio < 0.32f)
                biome = 2;
            else if (std::sin(ang * 3.0f + 1.1f) > 0.55f)
                biome = 3;

            /* Scattered clusters, not a solid carpet (preview cam reads carpets as atlas bars). */
            const int dens = (biome == 1) ? 28 : (biome == 2) ? 22 : (biome == 3) ? 20 : 14;
            if ((int)(h % 100) > dens)
                continue;

            const float y0 = groundY + 1.0f;
            const unsigned kind = (h >> 11) % 10;

            if (biome == 1 || (biome == 0 && kind < 3)) {
                /* Tree: 2–4 trunk blocks + 2×2 canopy layer + top block. */
                const int trunkH = 2 + (int)(h % 3);
                for (int t = 0; t < trunkH; ++t)
                    putBlock(x, y0 + (float)t * B, z, wood);
                const float cy = y0 + (float)trunkH * B;
                const DWORD lc = (h & 1) ? leafLit : leaf;
                putBlock(x, cy, z, lc);
                putBlock(x + B, cy, z, moss);
                putBlock(x - B, cy, z, lc);
                putBlock(x, cy, z + B, leaf);
                putBlock(x, cy, z - B, moss);
                putBlock(x, cy + B, z, lc);
            } else if (biome == 2 || kind == 4 || kind == 5) {
                /* Rock pillar stairs. */
                const int layers = 2 + (int)(h % 4);
                for (int L = 0; L < layers; ++L) {
                    putBlock(x, y0 + (float)L * B, z, (L & 1) ? stone : dark);
                    if (L > 0 && ((h >> L) & 1))
                        putBlock(x + B * 0.5f, y0 + (float)(L - 1) * B, z, stone);
                }
            } else if (biome == 3 || kind == 6) {
                /* Mesa terrace — khaki / cream steps (not rose / pure yellow). */
                const int steps = 2 + (int)(h % 3);
                for (int L = 0; L < steps; ++L) {
                    putBlock(x, y0 + (float)L * B, z, (L & 1) ? clay : sand);
                    if (L == 0)
                        putBlock(x + B, y0, z, sand);
                }
            } else if (kind == 7) {
                /* Hut: 2×2×2 stone + wood roof. */
                putBlock(x, y0, z, stone);
                putBlock(x + B, y0, z, stone);
                putBlock(x, y0 + B, z, stone);
                putBlock(x + B, y0 + B, z, wood);
            } else if (kind == 8) {
                /* Pond — recessed water block. */
                putBlock(x, y0 - B * 0.35f, z, water);
                putBlock(x + B, y0, z, dirt);
                putBlock(x - B, y0, z, grass);
            } else {
                /* Single grass/dirt block. */
                putBlock(x, y0, z, (h & 1) ? grass : dirt);
            }
        }
    }

done:
    pCubeFieldVB->Unlock();
    if (numCubeFieldVertices < 3 && pCubeFieldVB) {
        pCubeFieldVB->Release();
        pCubeFieldVB = NULL;
        numCubeFieldVertices = 0;
    }
}

static glm::vec3 PieceWorldVertex(long piece, long offset) {
    const long piece_x = Track[piece].x << (LOG_CUBE_SIZE - LOG_PRECISION);
    const long piece_y = Track[piece].y << (LOG_CUBE_SIZE - LOG_PRECISION);
    const long piece_z = Track[piece].z << (LOG_CUBE_SIZE - LOG_PRECISION);
    long x = Track[piece].coords[offset].x + piece_x;
    long y = Track[piece].coords[offset].y / 4 + piece_y;
    long z = Track[piece].coords[offset].z + piece_z;
    return glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

static float WorldCenter(void) {
    const long scale = (1L << (LOG_CUBE_SIZE - LOG_PRECISION));
    return static_cast<float>((NUM_TRACK_CUBES * scale) / 2);
}

void FreeTrackLifeVertexBuffer(void) {
    if (pLifeVB)
        pLifeVB->Release(), pLifeVB = NULL;
    numLifeVertices = 0;
    g_lifeStamp = -1.0f;
    ResetChaseDrones();
    ClearTrackLifeCarTargets();
}

void FreeTrackPropsVertexBuffer(void) {
    if (pPropsVB)
        pPropsVB->Release(), pPropsVB = NULL;
    numPropVertices = 0;
    if (pCubeFieldVB)
        pCubeFieldVB->Release(), pCubeFieldVB = NULL;
    numCubeFieldVertices = 0;
    FreeTrackLifeVertexBuffer();
}

void FreeAsphaltRoadVertexBuffer(void) {
    if (pAsphaltVB)
        pAsphaltVB->Release(), pAsphaltVB = NULL;
    numAsphaltVertices = 0;
    numAsphaltRoadTris = 0;
}

void RebuildAsphaltRoadVertexBuffer(RenderDevice* pDevice) {
    FreeAsphaltRoadVertexBuffer();
    if (pDevice == NULL || !IsAestheticsFeelEnabled() || TrackID == NO_TRACK)
        return;

    const long maxVerts = NumTrackPieces * MAX_SEGMENTS_PER_PIECE * 6;
    if (maxVerts <= 0)
        return;
    if (FAILED(pDevice->CreateVertexBuffer(maxVerts * sizeof(UTVERTEX), VB_USAGE_WRITEONLY, FVF_UTVERTEX, POOL_DEFAULT,
                                           &pAsphaltVB, NULL)))
        return;

    UTVERTEX* pVertices = NULL;
    if (FAILED(pAsphaltVB->Lock(0, 0, (void**)&pVertices, 0))) {
        FreeAsphaltRoadVertexBuffer();
        return;
    }

    numAsphaltVertices = 0;
    const DWORD white = 0xFFFFFFFFu;
    for (long piece = 0; piece < NumTrackPieces; ++piece) {
        if (Track[piece].coords == NULL)
            continue;
        const long numSegments = Track[piece].numSegments;
        for (long s = 0; s < numSegments; ++s) {
            const long offset = s * 4;
            glm::vec3 v0 = PieceWorldVertex(piece, offset);
            glm::vec3 v4 = PieceWorldVertex(piece, offset + 4);
            glm::vec3 v5 = PieceWorldVertex(piece, offset + 5);
            glm::vec3 v1 = PieceWorldVertex(piece, offset + 1);

            auto push = [&](const glm::vec3& p) {
                if (numAsphaltVertices >= maxVerts)
                    return;
                pVertices[numAsphaltVertices].pos = p;
                pVertices[numAsphaltVertices].color = white;
                pVertices[numAsphaltVertices].tu = p.x * ASPHALT_UV_SCALE;
                pVertices[numAsphaltVertices].tv = p.z * ASPHALT_UV_SCALE;
                ++numAsphaltVertices;
            };
            push(v0);
            push(v4);
            push(v5);
            push(v0);
            push(v5);
            push(v1);
        }
    }
    pAsphaltVB->Unlock();
    numAsphaltRoadTris = numAsphaltVertices / 3;
}

void DrawAsphaltRoad(RenderDevice* pDevice) {
    /* SCR roads are painted yellow/red panel stripes — not photoreal asphalt.
     * Enhanced Look relies on full-track atlas stripes in DrawTrack instead. */
    (void)pDevice;
}

void RebuildTrackProps(RenderDevice* pDevice) {
    FreeTrackPropsVertexBuffer();
    if (pDevice == NULL || !IsAestheticsFeelEnabled() || TrackID == NO_TRACK)
        return;

    if (FAILED(pDevice->CreateVertexBuffer(MAX_PROP_VERTICES * sizeof(UTVERTEX), VB_USAGE_WRITEONLY, FVF_UTVERTEX,
                                           POOL_DEFAULT, &pPropsVB, NULL)))
        return;

    UTVERTEX* pVertices = NULL;
    if (FAILED(pPropsVB->Lock(0, 0, (void**)&pVertices, 0))) {
        FreeTrackPropsVertexBuffer();
        return;
    }

    numPropVertices = 0;
    /* SCR track accents — yellow / dark-red / grey / white / real cyan (+6, not brick +11). */
    const DWORD barrierYellow = SCRGB(SCR_BASE_COLOUR + 3);
    const DWORD barrierRed = SCRGB(SCR_BASE_COLOUR + 9);
    const DWORD postGrey = SCRGB(SCR_BASE_COLOUR + 14);
    const DWORD flagWhite = SCRGB(SCR_BASE_COLOUR + 15);
    const DWORD accentCyan = SCRGB(SCR_BASE_COLOUR + 6);
    const DWORD pillarOlive = SCRGB(SCR_BASE_COLOUR + 13);
    const DWORD boardFace = SCRGB(SCR_BASE_COLOUR + 7); /* sky panel alt */
    const float flagPoleH = 128.0f;
    const float flagW = 56.0f;
    const float flagH = 36.0f;
    const float groundY = static_cast<float>(TRACK_BOTTOM_Y);

    for (long piece = 0; piece < NumTrackPieces; ++piece) {
        if (Track[piece].coords == NULL)
            continue;
        const long segs = Track[piece].numSegments;
        if (segs < 1)
            continue;

        const long mid = (segs / 2) * 4;
        glm::vec3 lMid0 = PieceWorldVertex(piece, mid);
        glm::vec3 lMid1 = PieceWorldVertex(piece, mid + 4);
        glm::vec3 rMid0 = PieceWorldVertex(piece, mid + 1);
        glm::vec3 rMid1 = PieceWorldVertex(piece, mid + 5);

        auto sideOut = [&](glm::vec3 a, glm::vec3 b, bool left) -> glm::vec3 {
            glm::vec3 dir = b - a;
            const float len = glm::length(dir);
            if (len < 1.0f)
                return glm::vec3(0.0f);
            dir /= len;
            glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0, 1, 0), dir));
            if (!left)
                side = -side;
            return side;
        };

        /* Continuous high side barriers — fixed height/outset, SCR yellow/red stripes. */
        const float kRailH = 36.0f;
        const float kRailOut = 12.0f;
        const float kRailTop = 3.0f;
        for (long s = 0; s < segs - 1; ++s) {
            const long offset = s * 4;
            glm::vec3 l0 = PieceWorldVertex(piece, offset);
            glm::vec3 l1 = PieceWorldVertex(piece, offset + 4);
            glm::vec3 r0 = PieceWorldVertex(piece, offset + 1);
            glm::vec3 r1 = PieceWorldVertex(piece, offset + 5);

            auto emitSegRail = [&](glm::vec3 a, glm::vec3 b, bool left) {
                glm::vec3 side = sideOut(a, b, left);
                if (glm::length(side) < 0.1f)
                    return;

                glm::vec3 a0 = a + side * kRailOut;
                glm::vec3 b0 = b + side * kRailOut;
                glm::vec3 a1 = a0 + glm::vec3(0, kRailH, 0);
                glm::vec3 b1 = b0 + glm::vec3(0, kRailH, 0);
                /* Two-segment stripe blocks so joins stay colour-matched. */
                const DWORD railCol = (((piece * 64 + s) / 2) % 2) ? barrierYellow : barrierRed;
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, a0, b0, b1, a1, railCol);
                /* Slim top cap for a clean continuous crown. */
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, a1, b1, b1 + glm::vec3(0, kRailTop, 0),
                         a1 + glm::vec3(0, kRailTop, 0), postGrey);

                /* Regular posts every 4 segments — same height as the barrier. */
                if ((s % 4) == 0) {
                    glm::vec3 p = a0;
                    glm::vec3 p2 = p + side * 5.0f;
                    EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, p, p2, p2 + glm::vec3(0, kRailH, 0),
                             p + glm::vec3(0, kRailH, 0), postGrey);
                }
            };

            emitSegRail(l0, l1, true);
            emitSegRail(r0, r1, false);
        }

        /* Support pillars under elevated deck. */
        if (true) {
            glm::vec3 deck = (lMid0 + rMid0) * 0.5f;
            const float deckY = deck.y;
            if (deckY > groundY + 80.0f) {
                const float half = 8.0f + static_cast<float>(PropHash(piece, 0, 41) % 8);
                glm::vec3 base(deck.x, groundY, deck.z);
                const DWORD pCol = (piece % 4) ? postGrey : pillarOlive;
                glm::vec3 t0 = base + glm::vec3(-half, 0, -half);
                glm::vec3 t1 = base + glm::vec3(half, 0, -half);
                glm::vec3 t2 = base + glm::vec3(half, 0, half);
                glm::vec3 t3 = base + glm::vec3(-half, 0, half);
                glm::vec3 u0 = t0 + glm::vec3(0, deckY - groundY - 4.0f, 0);
                glm::vec3 u1 = t1 + glm::vec3(0, deckY - groundY - 4.0f, 0);
                glm::vec3 u2 = t2 + glm::vec3(0, deckY - groundY - 4.0f, 0);
                glm::vec3 u3 = t3 + glm::vec3(0, deckY - groundY - 4.0f, 0);
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, t0, t1, u1, u0, pCol);
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, t1, t2, u2, u1, pCol);
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, t2, t3, u3, u2, pCol);
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, t3, t0, u0, u3, pCol);
            }
        }

        /* Flags on both sides — every other piece. */
        if ((piece % 2) == 0) {
            auto plantFlag = [&](bool left) {
                glm::vec3 side = sideOut(lMid0, lMid1, left);
                if (glm::length(side) < 0.1f)
                    return;
                glm::vec3 base = ((lMid0 + lMid1) * 0.5f) + side * 30.0f;
                glm::vec3 poleTop = base + glm::vec3(0, flagPoleH, 0);
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, base, base + side * 4.0f,
                         poleTop + side * 4.0f, poleTop, postGrey);

                glm::vec3 along = glm::normalize(lMid1 - lMid0);
                if (glm::length(lMid1 - lMid0) < 1.0f)
                    along = glm::vec3(1, 0, 0);
                const DWORD c0 = ((piece / 2 + (left ? 0 : 1)) % 2) ? barrierYellow : barrierRed;
                const DWORD c1 = ((piece / 2) % 2) ? barrierRed : flagWhite;
                glm::vec3 f0 = poleTop;
                glm::vec3 f1 = poleTop + along * flagW;
                glm::vec3 f2 = poleTop + along * flagW + glm::vec3(0, -flagH * 0.5f, 0);
                glm::vec3 f3 = poleTop + glm::vec3(0, -flagH, 0);
                EmitTri(pVertices, &numPropVertices, MAX_PROP_VERTICES, f0, f1, f2, c0);
                EmitTri(pVertices, &numPropVertices, MAX_PROP_VERTICES, f0, f2, f3, c1);
            };
            plantFlag(true);
            plantFlag(false);
        }

        /* Trackside arrow / distance boards. */
        if ((piece % 2) == 1) {
            glm::vec3 side = sideOut(rMid0, rMid1, false);
            if (glm::length(side) >= 0.1f) {
                glm::vec3 along = glm::normalize(rMid1 - rMid0);
                if (glm::length(rMid1 - rMid0) < 1.0f)
                    along = glm::vec3(1, 0, 0);
                glm::vec3 board = ((rMid0 + rMid1) * 0.5f) + side * 36.0f + glm::vec3(0, 40.0f, 0);
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, board - along * 18.0f,
                         board + along * 18.0f, board + along * 18.0f + glm::vec3(0, 28.0f, 0),
                         board - along * 18.0f + glm::vec3(0, 28.0f, 0), postGrey);
                EmitTri(pVertices, &numPropVertices, MAX_PROP_VERTICES, board + along * 14.0f + glm::vec3(0, 14.0f, 0),
                        board - along * 10.0f + glm::vec3(0, 22.0f, 0),
                        board - along * 10.0f + glm::vec3(0, 6.0f, 0), accentCyan);
            }
        }

        /* Dense 24h floodlights — both sides, every piece; thin mast, no wash cones. */
        {
            const float mastH = 130.0f + static_cast<float>(PropHash(piece, 0, 71) % 45);
            const float outSet = 28.0f;
            const float boomLen = 36.0f;
            auto plantFlood = [&](bool left) {
                glm::vec3 side = sideOut(lMid0, lMid1, left);
                if (glm::length(side) < 0.1f)
                    return;
                glm::vec3 along = lMid1 - lMid0;
                const float alen = glm::length(along);
                if (alen < 1.0f)
                    along = glm::vec3(1, 0, 0);
                else
                    along /= alen;

                glm::vec3 foot = ((lMid0 + lMid1) * 0.5f) + side * outSet;
                glm::vec3 mastTop = foot + glm::vec3(0, mastH, 0);
                const float hw = 2.0f;
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, foot - along * hw, foot + along * hw,
                         mastTop + along * hw, mastTop - along * hw, postGrey);

                glm::vec3 boomTip = mastTop - side * boomLen + glm::vec3(0, -14.0f, 0);
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, mastTop - along * 1.8f,
                         mastTop + along * 1.8f, boomTip + along * 1.8f, boomTip - along * 1.8f, postGrey);

                glm::vec3 hx = along * 8.0f;
                glm::vec3 hy = glm::vec3(0, 5.5f, 0);
                glm::vec3 hz = -side * 5.5f;
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, boomTip - hx - hy, boomTip + hx - hy,
                         boomTip + hx + hy, boomTip - hx + hy, postGrey);
                EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, boomTip - hx - hy * 0.2f + hz * 0.2f,
                         boomTip + hx - hy * 0.2f + hz * 0.2f, boomTip + hx - hy * 1.2f + hz * 1.4f,
                         boomTip - hx - hy * 1.2f + hz * 1.4f, barrierYellow);
            };
            plantFlood(true);
            plantFlood(false);
        }

        /* Mid-field brick clutter — tyre stacks, billboards, pylons, mini grandstands. */
        {
            glm::vec3 along = lMid1 - lMid0;
            const float alen = glm::length(along);
            if (alen >= 1.0f) {
                along /= alen;
                glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0, 1, 0), along));
                const DWORD tyreDark = SCRGB(SCR_BASE_COLOUR + 0);
                const DWORD tyreGrey = postGrey;
                const DWORD standCol = SCRGB(SCR_BASE_COLOUR + 10);
                const DWORD towerLit = barrierYellow;
                const DWORD towerAlt = barrierRed;

                for (int sideSign = -1; sideSign <= 1; sideSign += 2) {
                    const unsigned h = PropHash(piece, sideSign + 5, 101);
                    glm::vec3 mid = ((lMid0 + rMid0) * 0.5f);
                    const float out = 90.0f + static_cast<float>(h % 70);

                    /* Tyre stacks (Power Drift / pit-lane brick style). */
                    if ((h % 3) != 2) {
                        glm::vec3 stack = mid + side * (out * (float)sideSign) + along * ((float)(h % 17) - 8.0f);
                        stack.y = groundY + 2.0f;
                        const int layers = 2 + (int)(h % 3);
                        for (int L = 0; L < layers; ++L) {
                            const DWORD tc = (L & 1) ? tyreGrey : tyreDark;
                            EmitBox(pVertices, &numPropVertices, MAX_PROP_VERTICES,
                                    stack + glm::vec3(0, (float)L * 14.0f, 0), along, side, 16.0f, 12.0f, 12.0f, tc);
                        }
                    }

                    /* Big SCR billboard panels further out — upright only, not ground stickers. */
                    if ((piece + sideSign + (int)(h % 5)) % 3 == 0) {
                        glm::vec3 bill = mid + side * ((out + 55.0f) * (float)sideSign);
                        bill.y = groundY + 2.0f;
                        const float bh = 90.0f + static_cast<float>(h % 40);
                        EmitBox(pVertices, &numPropVertices, MAX_PROP_VERTICES, bill, along, side, 4.0f, 3.0f, bh,
                                postGrey);
                        glm::vec3 face = bill + side * (8.0f * (float)sideSign) + glm::vec3(0, bh * 0.25f, 0);
                        EmitQuad(pVertices, &numPropVertices, MAX_PROP_VERTICES, face - along * 28.0f,
                                 face + along * 28.0f, face + along * 28.0f + glm::vec3(0, bh * 0.55f, 0),
                                 face - along * 28.0f + glm::vec3(0, bh * 0.55f, 0),
                                 (h & 1) ? boardFace : barrierYellow);
                    }

                    /* Chequered start/finish style towers. */
                    if ((h % 4) == 0) {
                        glm::vec3 tower = mid + side * ((out + 20.0f) * (float)sideSign) + along * 30.0f;
                        tower.y = groundY + 2.0f;
                        const float th = 110.0f + static_cast<float>(h % 50);
                        EmitBox(pVertices, &numPropVertices, MAX_PROP_VERTICES, tower, along, side, 8.0f, 8.0f, th,
                                postGrey);
                        for (int band = 0; band < 4; ++band) {
                            const float by = th * 0.55f + (float)band * 12.0f;
                            EmitBox(pVertices, &numPropVertices, MAX_PROP_VERTICES, tower + glm::vec3(0, by, 0),
                                    along, side, 10.0f, 10.0f, 10.0f, (band & 1) ? towerLit : towerAlt);
                        }
                    }

                    /* Mini grandstand blocks (stepped). */
                    if ((piece % 3) == (sideSign > 0 ? 0 : 1)) {
                        glm::vec3 stand = mid + side * ((out + 95.0f) * (float)sideSign);
                        stand.y = groundY + 2.0f;
                        for (int step = 0; step < 3; ++step) {
                            EmitBox(pVertices, &numPropVertices, MAX_PROP_VERTICES,
                                    stand + side * ((float)step * 14.0f * (float)sideSign) +
                                        glm::vec3(0, (float)step * 16.0f, 0),
                                    along, side, 55.0f, 12.0f, 18.0f + (float)step * 6.0f,
                                    (step & 1) ? standCol : postGrey);
                        }
                    }
                }
            }
        }
    }

    pPropsVB->Unlock();
    RebuildCubeField(pDevice, groundY);
}

void UpdateTrackLife(RenderDevice* pDevice, float timeSeconds) {
    if (pDevice == NULL || !IsAestheticsFeelEnabled() || TrackID == NO_TRACK || NumTrackPieces <= 0) {
        numLifeVertices = 0;
        ResetChaseDrones();
        return;
    }

    /* Same-frame near/far passes share one life rebuild. */
    if (g_lifeStamp == timeSeconds && pLifeVB != NULL && numLifeVertices > 0)
        return;
    g_lifeStamp = timeSeconds;

    if (pLifeVB == NULL) {
        if (FAILED(pDevice->CreateVertexBuffer(MAX_LIFE_VERTICES * sizeof(UTVERTEX), VB_USAGE_WRITEONLY, FVF_UTVERTEX,
                                               POOL_DEFAULT, &pLifeVB, NULL)))
            return;
    }

    UTVERTEX* pVertices = NULL;
    if (FAILED(pLifeVB->Lock(0, 0, (void**)&pVertices, 0)))
        return;

    numLifeVertices = 0;
    const DWORD birdDark = SCRGB(SCR_BASE_COLOUR + 0);
    const DWORD birdYellow = SCRGB(SCR_BASE_COLOUR + 3);
    const DWORD birdWhite = SCRGB(SCR_BASE_COLOUR + 15);
    const DWORD dustGrey = SCRGB(SCR_BASE_COLOUR + 14);
    const DWORD dustOlive = SCRGB(SCR_BASE_COLOUR + 13);
    const DWORD beaconOn = SCRGB(SCR_BASE_COLOUR + 3);
    const DWORD beaconHot = SCRGB(SCR_BASE_COLOUR + 9);
    /* Cloud SCR accents — soft Amiga sky puffs (white / cream / grey / pale sky).
     * Avoid hot orange (+22) and brick (+11); those read as props, not clouds. */
    const DWORD cloudWhite = SCRGB(SCR_BASE_COLOUR + 15); /* #ffffff */
    const DWORD cloudCream = SCRGB(SCR_BASE_COLOUR + 2);  /* #bbbb99 */
    const DWORD cloudGrey = SCRGB(SCR_BASE_COLOUR + 14);  /* #bbbbbb */
    const DWORD cloudKhaki = SCRGB(SCR_BASE_COLOUR + 1);  /* #999977 soft shade */
    const DWORD cloudSky = SCRGB(SCR_BASE_COLOUR + 7);    /* #5599ff */
    const DWORD cloudCyan = SCRGB(SCR_BASE_COLOUR + 6);   /* #55bbff */
    const DWORD cloudBlush = SCRGB(SCR_BASE_COLOUR + 12); /* #dd9999 soft sunset only */
    const DWORD heatOrange = SCRGB(SCR_BASE_COLOUR + 22);
    const DWORD windYellow = SCRGB(SCR_BASE_COLOUR + 3);
    const DWORD windRed = SCRGB(SCR_BASE_COLOUR + 9);
    const float cx = WorldCenter();
    const float cz = WorldCenter();
    const float t = timeSeconds;

    auto lifeTri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, DWORD colour) {
        EmitTri(pVertices, &numLifeVertices, MAX_LIFE_VERTICES, a, b, c, colour);
    };

    auto emitBird = [&](float phase, float radius, float yBase, float size, DWORD col, float wingRate) {
        const float y = yBase + 60.0f * std::sin(t * 1.3f + phase * 3.0f);
        const float x = cx + radius * std::cos(phase);
        const float z = cz + radius * std::sin(phase);
        const float dx = -std::sin(phase);
        const float dz = std::cos(phase);
        const float wing = size * (1.1f + 0.35f * std::sin(t * wingRate + phase));
        const glm::vec3 tip(x + dx * size * 0.9f, y, z + dz * size * 0.9f);
        const glm::vec3 left(x - dx * size * 0.25f - dz * wing, y + size * 0.12f, z - dz * size * 0.25f + dx * wing);
        const glm::vec3 right(x - dx * size * 0.25f + dz * wing, y + size * 0.12f, z - dz * size * 0.25f - dx * wing);
        lifeTri(tip, left, right, col);
    };

    /* Multi-lobe SCR cloud: slow altitude-dependent drift, layered volume, band palette. */
    auto emitCloud = [&](float phase, float radius, float yBase, float scale, int seed, float driftRate,
                         float bobRate, float evolveRate, DWORD lit, DWORD mid, DWORD shade, DWORD rim) {
        /* Slow orbit — lower bands drift a bit faster than high wisps. */
        const float drift = t * driftRate * (0.75f + 0.08f * (float)(seed % 5));
        const float ang = phase + drift;
        /* Gentle vertical bob + very slow radius “evolution”. */
        const float bob = (28.0f + 0.04f * scale) * std::sin(t * bobRate + (float)seed * 0.7f);
        const float evolve = 1.0f + 0.06f * std::sin(t * evolveRate + (float)seed * 0.4f);
        const float breathe = 1.0f + 0.035f * std::sin(t * (bobRate * 1.4f) + (float)seed);
        const float rad = radius * (1.0f + 0.012f * std::sin(t * evolveRate * 0.7f + (float)seed));
        const float x = cx + rad * std::cos(ang);
        const float z = cz + rad * std::sin(ang);
        const float y = yBase + bob;

        glm::vec3 inward(cx - x, 0.0f, cz - z);
        const float inLen = glm::length(inward);
        if (inLen < 1.0f)
            inward = glm::vec3(1, 0, 0);
        else
            inward /= inLen;
        glm::vec3 east = glm::normalize(glm::cross(glm::vec3(0, 1, 0), inward));
        glm::vec3 north = glm::normalize(glm::cross(glm::vec3(0, 1, 0), east));
        const float sMul = scale * breathe * evolve;
        east *= sMul;
        north *= sMul;

        struct Lobe {
            float ox, oz, s;
            int shadeIdx;
        };
        static const Lobe kLobes[] = {
            {0.00f, 0.05f, 1.05f, 1},  {-0.78f, 0.00f, 0.78f, 0}, {0.82f, 0.04f, 0.74f, 0},
            {-0.32f, 0.48f, 0.62f, 0}, {0.36f, 0.44f, 0.60f, 1},  {-1.05f, -0.14f, 0.52f, 2},
            {1.08f, -0.12f, 0.50f, 2}, {0.00f, -0.34f, 0.78f, 3}, {-0.52f, 0.26f, 0.48f, 1},
            {0.55f, 0.22f, 0.46f, 0},  {-0.15f, -0.55f, 0.42f, 2}, {0.18f, 0.62f, 0.40f, 0},
        };
        const DWORD shades[4] = {lit, mid, shade, rim};

        struct Layer {
            float yFrac;
            float size;
            float shadeBias;
        };
        static const Layer kLayers[] = {
            {-0.22f, 1.15f, 2.0f}, {-0.08f, 1.05f, 1.0f}, {0.00f, 1.00f, 0.0f},
            {0.12f, 0.88f, 0.0f},  {0.24f, 0.68f, 1.0f}, {0.36f, 0.48f, 0.0f},
        };

        auto lobeDiamond = [&](float ox, float oz, float s, float yLift, DWORD col) {
            const glm::vec3 o = glm::vec3(x, y + yLift, z) + east * ox + north * oz;
            const glm::vec3 r = east * s;
            const glm::vec3 n = north * s;
            lifeTri(o, o + r, o + n * 0.85f, col);
            lifeTri(o, o + n * 0.85f, o - r, col);
            lifeTri(o, o - r, o - n * 0.55f, col);
            lifeTri(o, o - n * 0.55f, o + r, col);
        };

        for (const Layer& layer : kLayers) {
            const float yLift = layer.yFrac * scale;
            const float layerScale = layer.size;
            for (const Lobe& lobe : kLobes) {
                if (layer.yFrac > 0.2f && lobe.s < 0.5f)
                    continue;
                if (layer.yFrac < -0.15f && lobe.s < 0.55f)
                    continue;
                const float wobble =
                    0.035f * std::sin(t * (evolveRate * 2.2f) + lobe.ox * 4.0f + (float)seed + layer.yFrac);
                int shadeIdx = lobe.shadeIdx + (int)layer.shadeBias;
                if (shadeIdx > 3)
                    shadeIdx = 3;
                if (shadeIdx < 0)
                    shadeIdx = 0;
                lobeDiamond(lobe.ox * layerScale + wobble, lobe.oz * layerScale, lobe.s * layerScale, yLift,
                            shades[shadeIdx]);
            }
        }

        {
            const glm::vec3 o = glm::vec3(x, y - 0.28f * scale, z);
            const glm::vec3 r = east * 1.25f;
            const glm::vec3 n = north * 1.0f;
            lifeTri(o - r - n, o + r - n, o + r * 0.2f + n, shade);
            lifeTri(o - r - n, o + r * 0.2f + n, o - r * 0.4f + n, rim);
        }
    };

    /* Three flocks: denser orbits. */
    for (int i = 0; i < 22; ++i) {
        const float phase = t * 0.42f + (float)i * 0.29f;
        emitBird(phase, 16000.0f + (float)(i % 5) * 2200.0f, 380.0f + (float)(i % 3) * 40.0f, 48.0f,
                 (i & 1) ? birdDark : birdYellow, 9.0f);
    }
    for (int i = 0; i < 18; ++i) {
        const float phase = -t * 0.28f + (float)i * 0.35f;
        emitBird(phase, 22000.0f + (float)(i % 4) * 2800.0f, 700.0f + (float)(i % 4) * 55.0f, 70.0f, birdYellow,
                 7.0f);
    }
    for (int i = 0; i < 12; ++i) {
        const float phase = t * 0.18f + (float)i * 0.52f;
        emitBird(phase, 30000.0f + (float)i * 700.0f, 1100.0f + 40.0f * std::sin(t + (float)i), 95.0f, birdWhite,
                 5.5f);
    }

    /* Near / preview band: cream–white with a soft sky rim. */
    for (int i = 0; i < 10; ++i) {
        const float phase = (float)i * 0.628f + 2.4f;
        const float radius = 9000.0f + (float)((i * 1900) % 11000);
        const float y = 1400.0f + (float)(i % 4) * 160.0f;
        const float scale = 480.0f + (float)(i % 3) * 110.0f;
        emitCloud(phase, radius, y, scale, i * 13 + 5, 0.014f, 0.13f, 0.09f, cloudWhite, cloudCream, cloudGrey,
                  cloudSky);
    }
    /* Altitude bands: warm low / soft mid / cool high — each drifts at its own pace. */
    for (int i = 0; i < 12; ++i) {
        const float phase = (float)i * 0.5236f + 0.15f;
        const float radius = 21000.0f + (float)((i * 2400) % 7000);
        const float y = 2400.0f + (float)(i % 3) * 180.0f;
        const float scale = 720.0f + (float)(i % 4) * 140.0f;
        /* Low: cream belly, light blush on shade, not hot orange. */
        emitCloud(phase, radius, y, scale, i * 17 + 3, 0.010f, 0.11f, 0.07f, cloudWhite, cloudCream, cloudBlush,
                  cloudKhaki);
    }
    for (int i = 0; i < 11; ++i) {
        const float phase = (float)i * 0.571f + 1.1f;
        const float radius = 30000.0f + (float)((i * 3100) % 9000);
        const float y = 3200.0f + (float)(i % 4) * 200.0f;
        const float scale = 920.0f + (float)(i % 3) * 160.0f;
        /* Mid: white / grey / cream / soft sky. */
        emitCloud(phase, radius, y, scale, i * 23 + 11, 0.0055f, 0.08f, 0.05f, cloudWhite, cloudGrey, cloudCream,
                  cloudSky);
    }
    for (int i = 0; i < 10; ++i) {
        const float phase = (float)i * 0.628f + 0.4f;
        const float radius = 40000.0f + (float)((i * 2800) % 8000);
        const float y = 4200.0f + (float)(i % 3) * 260.0f;
        const float scale = 1100.0f + (float)(i % 2) * 200.0f;
        /* High: cool cyan / sky wisps, almost still. */
        emitCloud(phase, radius, y, scale, i * 31 + 7, 0.0022f, 0.05f, 0.035f, cloudWhite, cloudCyan, cloudSky,
                  cloudGrey);
    }

    /* Dense rising dust + heat shimmer. */
    for (long piece = 0; piece < NumTrackPieces; ++piece) {
        if (Track[piece].coords == NULL)
            continue;
        const long m = piece % 7;
        const bool bumpy = (m == 2 || m == 3 || m == 5);
        const long segs = Track[piece].numSegments;
        if (segs < 1)
            continue;
        const long mid = (segs / 2) * 4;
        glm::vec3 base = PieceWorldVertex(piece, mid);
        const int moteCount = bumpy ? 7 : 3;
        for (int k = 0; k < moteCount; ++k) {
            const float seed = (float)piece * 17.0f + (float)k * 41.0f;
            const float bob = std::fmod(t * (28.0f + (float)k * 8.0f) + seed, bumpy ? 110.0f : 70.0f);
            const float sway = 18.0f * std::sin(t * 2.4f + seed);
            glm::vec3 p = base;
            p.x += sway + (float)(k - 2) * 22.0f;
            p.y += 12.0f + bob;
            p.z += sway * 0.4f + (float)k * 8.0f;
            const float s = (bumpy ? 12.0f : 8.0f) + 5.0f * std::sin(t * 3.5f + seed);
            const DWORD col = ((piece + k) & 1) ? dustGrey : dustOlive;
            lifeTri(p + glm::vec3(0, s, 0), p + glm::vec3(-s, 0, 0), p + glm::vec3(s, 0, 0), col);
            if (bumpy && k == 0) {
                glm::vec3 h = base + glm::vec3(0, 8.0f + 20.0f * std::sin(t * 6.0f + seed), 0);
                lifeTri(h + glm::vec3(0, 8, 0), h + glm::vec3(-6, 0, 0), h + glm::vec3(6, 0, 0), heatOrange);
            }
        }
    }

    /* Rolling tumble-dust along both outsides. */
    for (long piece = 0; piece < NumTrackPieces; ++piece) {
        if (Track[piece].coords == NULL)
            continue;
        const long segs = Track[piece].numSegments;
        if (segs < 1)
            continue;
        const long mid = (segs / 2) * 4;
        glm::vec3 l0 = PieceWorldVertex(piece, mid);
        glm::vec3 l1 = PieceWorldVertex(piece, mid + 4);
        glm::vec3 dir = l1 - l0;
        const float len = glm::length(dir);
        if (len < 1.0f)
            continue;
        dir /= len;
        glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0, 1, 0), dir));
        for (int sideSign = -1; sideSign <= 1; sideSign += 2) {
            const float roll = std::fmod(t * 55.0f + (float)piece * 13.0f + (float)sideSign * 40.0f, len + 80.0f);
            glm::vec3 p = l0 + dir * roll + side * (48.0f * (float)sideSign) + glm::vec3(0, 10.0f, 0);
            const float ang = t * 6.0f + (float)piece + (float)sideSign;
            const float rr = 14.0f;
            lifeTri(p + glm::vec3(std::cos(ang) * rr, std::sin(ang) * rr, 0),
                    p + glm::vec3(std::cos(ang + 2.1f) * rr, std::sin(ang + 2.1f) * rr, 0),
                    p + glm::vec3(std::cos(ang + 4.2f) * rr, std::sin(ang + 4.2f) * rr, 0), dustOlive);
        }
    }

    /* Wind ribbons on every piece, both edges. */
    for (long piece = 0; piece < NumTrackPieces; ++piece) {
        if (Track[piece].coords == NULL)
            continue;
        const long segs = Track[piece].numSegments;
        if (segs < 1)
            continue;
        const long mid = (segs / 2) * 4;
        glm::vec3 l0 = PieceWorldVertex(piece, mid);
        glm::vec3 l1 = PieceWorldVertex(piece, mid + 4);
        glm::vec3 dir = l1 - l0;
        const float len = glm::length(dir);
        if (len < 1.0f)
            continue;
        dir /= len;
        glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0, 1, 0), dir));
        const float wave = 8.0f * std::sin(t * 4.0f + (float)piece * 0.9f);
        const float travel = std::fmod(t * 90.0f + (float)piece * 20.0f, len);
        for (int sideSign = -1; sideSign <= 1; sideSign += 2) {
            glm::vec3 a = l0 + dir * travel + side * (22.0f * (float)sideSign) + glm::vec3(0, 28.0f + wave, 0);
            glm::vec3 b = a + dir * 55.0f + glm::vec3(0, wave * 0.5f, 0);
            glm::vec3 c = a + side * (6.0f * (float)sideSign) + glm::vec3(0, -6.0f, 0);
            lifeTri(a, b, c, (sideSign > 0) ? windYellow : windRed);
        }
    }

    /* Dense flood glow — both sides every piece, random blink, flat spill only. */
    const DWORD lampHot = SCRGB(SCR_BASE_COLOUR + 15);
    const DWORD lampWarm = SCRGB(SCR_BASE_COLOUR + 3);
    const DWORD lampAmber = SCRGB(SCR_BASE_COLOUR + 22);
    const DWORD lampOff = SCRGB(SCR_BASE_COLOUR + 14);
    for (long piece = 0; piece < NumTrackPieces; ++piece) {
        if (Track[piece].coords == NULL)
            continue;
        const long segs = Track[piece].numSegments;
        if (segs < 1)
            continue;
        const long mid = (segs / 2) * 4;
        glm::vec3 l0 = PieceWorldVertex(piece, mid);
        glm::vec3 l1 = PieceWorldVertex(piece, mid + 4);
        glm::vec3 dir = l1 - l0;
        const float len = glm::length(dir);
        if (len < 1.0f)
            continue;
        dir /= len;
        glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0, 1, 0), dir));
        const float mastH = 130.0f + static_cast<float>(PropHash(piece, 0, 71) % 45);
        const float outSet = 28.0f;
        const float boomLen = 36.0f;

        for (int sideSign = -1; sideSign <= 1; sideSign += 2) {
            const unsigned seed = PropHash(piece, sideSign + 4, 91);
            const float period = 0.45f + static_cast<float>(seed % 27) * 0.10f;
            const float duty = 0.32f + static_cast<float>((seed >> 5) % 10) * 0.05f;
            const float phaseOff = static_cast<float>((seed >> 9) % 64) * 0.041f;
            const float cycle = std::fmod(t / period + phaseOff, 1.0f);
            const bool on = cycle < duty;

            glm::vec3 foot = ((l0 + l1) * 0.5f) + side * (outSet * (float)sideSign);
            glm::vec3 mastTop = foot + glm::vec3(0, mastH, 0);
            glm::vec3 lamp = mastTop - side * (boomLen * (float)sideSign) + glm::vec3(0, -14.0f, 0);

            if (on) {
                const float coreR = 15.0f + 4.0f * std::sin(t * 18.0f + (float)piece + (float)sideSign);
                const float haloR = 28.0f;
                lifeTri(lamp + glm::vec3(0, coreR, 0), lamp + dir * coreR, lamp - dir * coreR, lampHot);
                lifeTri(lamp + glm::vec3(0, haloR * 0.3f, 0),
                        lamp + dir * haloR + side * (5.0f * (float)sideSign),
                        lamp - dir * haloR + side * (5.0f * (float)sideSign), lampWarm);
                glm::vec3 deckHit =
                    ((l0 + l1) * 0.5f) - side * (6.0f * (float)sideSign) + glm::vec3(0, 3.0f, 0);
                const float pool = 36.0f;
                lifeTri(deckHit, deckHit + dir * pool, deckHit - dir * pool, lampWarm);
                lifeTri(deckHit, deckHit + dir * (pool * 0.5f) - side * (16.0f * (float)sideSign),
                        deckHit - dir * (pool * 0.5f) - side * (16.0f * (float)sideSign), lampAmber);
            } else {
                const float r = 5.5f;
                lifeTri(lamp + glm::vec3(0, r, 0), lamp + dir * r, lamp - dir * r, lampOff);
            }
        }
    }

    /* Marker blinkers on every piece — random. */
    for (long piece = 0; piece < NumTrackPieces; ++piece) {
        if (Track[piece].coords == NULL)
            continue;
        const long segs = Track[piece].numSegments;
        if (segs < 1)
            continue;
        const long mid = (segs / 2) * 4;
        glm::vec3 l0 = PieceWorldVertex(piece, mid);
        glm::vec3 l1 = PieceWorldVertex(piece, mid + 4);
        glm::vec3 dir = l1 - l0;
        const float len = glm::length(dir);
        if (len < 1.0f)
            continue;
        dir /= len;
        glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0, 1, 0), dir));
        for (int sideSign = -1; sideSign <= 1; sideSign += 2) {
            const unsigned seed = PropHash(piece, sideSign + 3, 53);
            const float period = 0.35f + static_cast<float>(seed % 21) * 0.08f;
            const float duty = 0.22f + static_cast<float>((seed >> 4) % 9) * 0.05f;
            const float phaseOff = static_cast<float>((seed >> 8) % 50) * 0.05f;
            const float cycle = std::fmod(t / period + phaseOff, 1.0f);
            if (cycle > duty)
                continue;
            const float r = 9.0f + 5.0f * (1.0f - cycle / duty);
            const DWORD col = (seed & 1) ? beaconOn : beaconHot;
            glm::vec3 tip = ((l0 + l1) * 0.5f) + side * (20.0f * (float)sideSign) + glm::vec3(0, 52.0f, 0);
            lifeTri(tip + glm::vec3(0, r, 0), tip + dir * r, tip - dir * r, col);
        }
    }

    /* Flag flutter every other piece. */
    for (long piece = 0; piece < NumTrackPieces; piece += 2) {
        if (Track[piece].coords == NULL)
            continue;
        const long segs = Track[piece].numSegments;
        if (segs < 1)
            continue;
        const long mid = (segs / 2) * 4;
        glm::vec3 l0 = PieceWorldVertex(piece, mid);
        glm::vec3 l1 = PieceWorldVertex(piece, mid + 4);
        glm::vec3 dir = l1 - l0;
        const float len = glm::length(dir);
        if (len < 1.0f)
            continue;
        dir /= len;
        glm::vec3 side = glm::normalize(glm::cross(glm::vec3(0, 1, 0), dir));
        for (int sideSign = -1; sideSign <= 1; sideSign += 2) {
            const float flap = 14.0f * std::sin(t * 5.5f + (float)piece * 0.7f + (float)sideSign);
            const float flap2 = 8.0f * std::sin(t * 7.0f + (float)piece + 1.2f);
            glm::vec3 base = ((l0 + l1) * 0.5f) + side * (30.0f * (float)sideSign) + glm::vec3(0, 110.0f, 0);
            glm::vec3 tip = base + dir * (48.0f + flap) + side * (flap * 0.4f * (float)sideSign);
            glm::vec3 midF = base + dir * (24.0f + flap2 * 0.5f) + glm::vec3(0, -16.0f, 0) +
                             side * (flap2 * 0.25f * (float)sideSign);
            const DWORD c0 = ((piece / 2 + sideSign) & 1) ? birdYellow : beaconHot;
            const DWORD c1 = birdWhite;
            lifeTri(base, tip, midF, c0);
            lifeTri(base, midF, base + glm::vec3(0, -32.0f, 0), c1);
        }
    }

    /* Dense horizon sparkles. */
    for (int i = 0; i < 40; ++i) {
        const float phase = (float)i * (6.2831853f / 40.0f);
        const float pulse = std::fmod(t * 1.7f + (float)i * 0.17f, 1.0f);
        if (pulse > 0.6f)
            continue;
        const float radius = 36000.0f + (float)((i % 5) * 1200);
        const float y = 180.0f + 90.0f * (pulse < 0.3f ? 1.0f : 0.4f);
        const float x = cx + radius * std::cos(phase + t * 0.02f);
        const float z = cz + radius * std::sin(phase + t * 0.02f);
        const float r = 36.0f + 18.0f * (1.0f - pulse);
        const DWORD col = (i & 1) ? beaconOn : beaconHot;
        lifeTri(glm::vec3(x, y + r, z), glm::vec3(x - r, y, z), glm::vec3(x + r, y, z), col);
    }

    /* Chase drones — max 2 per track; sticky-assign to player / rivals. */
    {
        float dt = 1.0f / 60.0f;
        if (g_dronePrevTime >= 0.0f) {
            dt = t - g_dronePrevTime;
            if (dt < 0.0f || dt > 0.25f)
                dt = 1.0f / 60.0f;
        }
        g_dronePrevTime = t;

        const int targetCount = g_carTargetCount;
        const int activeDrones = (targetCount <= 0) ? 0 : kChaseDroneCount;

        if (activeDrones == 0) {
            for (int d = 0; d < kChaseDroneCount; ++d)
                g_drones[d].alive = false;
            g_dronesBooted = false;
        } else {
            if (!g_dronesBooted) {
                for (int d = 0; d < kChaseDroneCount; ++d) {
                    ChaseDrone& dr = g_drones[d];
                    const int ti = (targetCount >= 2) ? (d % targetCount) : 0;
                    const LifeCarTarget& car = g_carTargets[ti];
                    const float fwdX = std::sin(car.yaw);
                    const float fwdZ = std::cos(car.yaw);
                    const float rightX = fwdZ;
                    const float rightZ = -fwdX;
                    dr.sideSign = (d == 0) ? -1.0f : 1.0f;
                    dr.hoverBias = 88.0f + 22.0f * (float)d;
                    dr.leadBias = 40.0f + 55.0f * (float)d;
                    dr.targetIndex = ti;
                    dr.x = car.x + rightX * (dr.sideSign * 95.0f) - fwdX * 40.0f;
                    dr.y = car.y + dr.hoverBias;
                    dr.z = car.z + rightZ * (dr.sideSign * 95.0f) - fwdZ * 40.0f;
                    dr.yaw = car.yaw;
                    dr.rotor = (float)d * 1.7f;
                    dr.alive = true;
                }
                g_dronesBooted = true;
            }

            const DWORD hull = SCRGB(SCR_BASE_COLOUR + 14);  /* grey */
            const DWORD arm = SCRGB(SCR_BASE_COLOUR + 0);     /* dark */
            const DWORD accent = SCRGB(SCR_BASE_COLOUR + 6);  /* cyan */
            const DWORD tipOn = SCRGB(SCR_BASE_COLOUR + 15);  /* white */
            const DWORD tipHot = SCRGB(SCR_BASE_COLOUR + 3);  /* yellow */
            const DWORD lens = SCRGB(SCR_BASE_COLOUR + 9);    /* red LED */

            for (int d = 0; d < kChaseDroneCount; ++d) {
                ChaseDrone& dr = g_drones[d];
                /* Sticky target: prefer distinct cars when 2+ exist. */
                int ti = dr.targetIndex;
                if (ti < 0 || ti >= targetCount)
                    ti = (targetCount >= 2) ? (d % targetCount) : 0;
                if (targetCount >= 2) {
                    const int preferred = d % targetCount;
                    if (ti == g_drones[1 - d].targetIndex && preferred != ti)
                        ti = preferred;
                }
                dr.targetIndex = ti;
                const LifeCarTarget& car = g_carTargets[ti];

                const float fwdX = std::sin(car.yaw);
                const float fwdZ = std::cos(car.yaw);
                const float rightX = fwdZ;
                const float rightZ = -fwdX;
                const float seed = (float)d * 2.1f + 0.35f;
                const float leadWave = std::sin(t * (0.22f + 0.05f * (float)d) + seed);
                const float lead = dr.leadBias + 160.0f * leadWave;
                const float side =
                    dr.sideSign * (85.0f + 28.0f * (float)d) + 22.0f * std::sin(t * 0.7f + seed * 1.8f);
                const float hover = dr.hoverBias + 14.0f * std::sin(t * 1.55f + seed);

                const float wantX = car.x + fwdX * lead + rightX * side;
                const float wantY = car.y + hover;
                const float wantZ = car.z + fwdZ * lead + rightZ * side;

                const float follow = 1.0f - std::exp(-4.2f * dt);
                const float turnFollow = 1.0f - std::exp(-5.5f * dt);
                dr.x += (wantX - dr.x) * follow;
                dr.y += (wantY - dr.y) * follow;
                dr.z += (wantZ - dr.z) * follow;

                float toCarX = car.x - dr.x;
                float toCarZ = car.z - dr.z;
                const float toLen = std::sqrt(toCarX * toCarX + toCarZ * toCarZ);
                float faceYaw = car.yaw;
                if (toLen > 8.0f)
                    faceYaw = std::atan2(toCarX, toCarZ);
                float dyaw = faceYaw - dr.yaw;
                while (dyaw > 3.14159265f)
                    dyaw -= 6.2831853f;
                while (dyaw < -3.14159265f)
                    dyaw += 6.2831853f;
                dr.yaw += dyaw * turnFollow;
                dr.rotor += dt * (14.0f + 2.5f * (float)d);
                dr.alive = true;

                const float dx = dr.x;
                const float dy = dr.y;
                const float dz = dr.z;
                const float fX = std::sin(dr.yaw);
                const float fZ = std::cos(dr.yaw);
                const float rX = fZ;
                const float rZ = -fX;

                /* Oriented SCR camera-drone: body, arms, rotors, lens toward car. */
                const float halfL = 16.0f;
                const float halfW = 10.0f;
                const glm::vec3 nose(dx + fX * halfL, dy, dz + fZ * halfL);
                const glm::vec3 tail(dx - fX * halfL * 0.85f, dy + 2.0f, dz - fZ * halfL * 0.85f);
                const glm::vec3 left(dx - rX * halfW, dy + 1.0f, dz - rZ * halfW);
                const glm::vec3 right(dx + rX * halfW, dy + 1.0f, dz + rZ * halfW);
                lifeTri(nose, left, right, hull);
                lifeTri(tail, right, left, accent);

                const float armLen = 26.0f;
                const float armY = dy + 5.0f;
                for (int a = 0; a < 4; ++a) {
                    const float sx = ((a & 1) ? 1.0f : -1.0f);
                    const float sz = ((a & 2) ? 1.0f : -1.0f);
                    const float ax = dx + (fX * sx + rX * sz) * armLen * 0.55f;
                    const float az = dz + (fZ * sx + rZ * sz) * armLen * 0.55f;
                    lifeTri(glm::vec3(dx, armY, dz), glm::vec3(ax - rX * 2.0f, armY, az - rZ * 2.0f),
                            glm::vec3(ax + rX * 2.0f, armY, az + rZ * 2.0f), arm);

                    const float spin = dr.rotor + (float)a * 1.5707963f;
                    const float rr = 18.0f;
                    const float bx = std::cos(spin) * rr;
                    const float bz = std::sin(spin) * rr;
                    /* Rotor blade in world XZ, centered on arm tip. */
                    lifeTri(glm::vec3(ax, armY + 3.0f, az), glm::vec3(ax + bx, armY + 3.0f, az + bz),
                            glm::vec3(ax - bz * 0.35f, armY + 3.0f, az + bx * 0.35f), tipOn);
                    lifeTri(glm::vec3(ax, armY + 3.0f, az), glm::vec3(ax - bx, armY + 3.0f, az - bz),
                            glm::vec3(ax + bz * 0.35f, armY + 3.0f, az - bx * 0.35f), tipHot);
                }

                /* Camera gondola + LED facing the tracked car. */
                const float camX = dx + fX * 6.0f;
                const float camY = dy - 10.0f;
                const float camZ = dz + fZ * 6.0f;
                lifeTri(glm::vec3(camX, camY + 5.0f, camZ), glm::vec3(camX - rX * 5.0f, camY, camZ - rZ * 5.0f),
                        glm::vec3(camX + rX * 5.0f, camY, camZ + rZ * 5.0f), arm);
                const float blink = 0.5f + 0.5f * std::sin(t * 10.0f + seed);
                const DWORD led = (blink > 0.35f) ? lens : tipHot;
                const float tip = 4.5f + 2.0f * blink;
                lifeTri(glm::vec3(camX + fX * 8.0f, camY + tip * 0.2f, camZ + fZ * 8.0f),
                        glm::vec3(camX - rX * tip, camY, camZ - rZ * tip),
                        glm::vec3(camX + rX * tip, camY, camZ + rZ * tip), led);
            }
        }
    }

    pLifeVB->Unlock();
}

void DrawTrackProps(RenderDevice* pDevice) {
    if (!IsAestheticsFeelEnabled() || pDevice == NULL)
        return;

    pDevice->SetRenderState(RS_ZENABLE, TRUE);
    pDevice->SetRenderState(RS_CULLMODE, CULL_NONE);
    pDevice->SetTexture(0, NULL);
    pDevice->SetTextureStageState(0, TSS_COLOROP, TOP_DISABLE);
    pDevice->SetLitMaterial(false);
    pDevice->SetFVF(FVF_UTVERTEX);

    if (pCubeFieldVB != NULL && numCubeFieldVertices >= 3) {
        pDevice->SetStreamSource(0, pCubeFieldVB, 0, sizeof(UTVERTEX));
        pDevice->DrawPrimitive(PT_TRIANGLELIST, 0, numCubeFieldVertices / 3);
    }
    if (pPropsVB != NULL && numPropVertices >= 3) {
        pDevice->SetStreamSource(0, pPropsVB, 0, sizeof(UTVERTEX));
        pDevice->DrawPrimitive(PT_TRIANGLELIST, 0, numPropVertices / 3);
    }
}

void DrawTrackLife(RenderDevice* pDevice) {
    if (!IsAestheticsFeelEnabled() || pDevice == NULL || pLifeVB == NULL || numLifeVertices < 3)
        return;

    pDevice->SetRenderState(RS_ZENABLE, TRUE);
    pDevice->SetRenderState(RS_CULLMODE, CULL_NONE);
    pDevice->SetTexture(0, NULL);
    pDevice->SetTextureStageState(0, TSS_COLOROP, TOP_DISABLE);
    pDevice->SetLitMaterial(false);
    pDevice->SetStreamSource(0, pLifeVB, 0, sizeof(UTVERTEX));
    pDevice->SetFVF(FVF_UTVERTEX);
    pDevice->DrawPrimitive(PT_TRIANGLELIST, 0, numLifeVertices / 3);
}
