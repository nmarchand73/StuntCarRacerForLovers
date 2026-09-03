
#ifndef _TRACK
#define _TRACK

/*    ============= */
/*    Include files */
/*    ============= */
#include "3D_Engine.h"
#include "platform_sdl_gl.h"

/*    ========= */
/*    Constants */
/*    ========= */
#define PC_FACTOR 2 // 24/04/1998

#define MAX_PIECES_PER_TRACK 100

#define MAX_SEGMENTS_PER_PIECE 13

#define NUM_TRACK_CUBES 16 // x,z dimensions of Track, in cubes

#define CUBE_SIZE                                                                                                      \
    0x04000000 // size of a single track cube                                                                \
                         // (0x800 * PC_FACTOR of 2 * PRECISION)
#define LOG_CUBE_SIZE 26 // to base 2

#define TRACK_BOTTOM_Y 0 // used to be 512

#define GROUND_PLANE_WIDTH (NUM_TRACK_CUBES * CUBE_SIZE)
#define GROUND_PLANE_DEPTH (NUM_TRACK_CUBES * CUBE_SIZE)

// note that track numbers do not correspond to track league positions
#define NO_TRACK -1
#define LITTLE_RAMP 0
#define STEPPING_STONES 1
#define HUMP_BACK 2
#define BIG_RAMP 3
#define SKI_JUMP 4
#define DRAW_BRIDGE 5
#define HIGH_JUMP 6
#define ROLLER_COASTER 7
#define NUM_TRACKS 8

typedef enum {
    TRACK_PACK_CLASSIC = 0,
    TRACK_PACK_TNT = 1,
    TRACK_PACK_ORIGINAL = 2,
    TRACK_PACK_LOOPS = 3,
    NUM_TRACK_PACKS = 4
} TrackPack;

/** Amiga practice / league divisions (display numbers 1–4). Classic pack only. */
#define NUM_TRACK_DIVISIONS 4
#define TRACKS_PER_DIVISION 2

/*    ===================== */
/*    Structure definitions */
/*    ===================== */
typedef struct {
    long x;
    long z;
} COORD_XZ;

typedef struct {
    long y;
} COORD_Y;

typedef struct {
    long x, y, z;           // of piece's front left corner, within world
    long roughPieceAngle;   // 0, 90, 180, 270 degrees (stored in internal angle format)
    long oppositeDirection; // TRUE/FALSE - normal travel direction along piece
    long curveToLeft;       // TRUE/FALSE
    long type;              // near.section.byte1 - 0x00 STRAIGHT, 0x40 DIAGONAL (45 degrees)
                            //                        0x80 CURVE RIGHT, 0xc0 CURVE LEFT
    long lengthReduction;
    long steeringAmount;
    /* following not needed since project converted to use Direct3D
    long    visible;        // TRUE/FALSE - from current viewpoint
    long    distant;        // TRUE/FALSE - from current viewpoint (using DISTANT_OBJECT_BOUNDARY)
    long    zClipNeeded;    // TRUE/FALSE - from current viewpoint
    */
    long numSegments;
    long firstSegment; // piece's first segment number within the whole track
    /* following not needed since project converted to use Direct3D
    long    minZDifference;    // used to decide order of segment drawing
    */
    long initialColour;                      // initial roadLinesColour offset (0 or 1)
    BYTE roadColour[MAX_SEGMENTS_PER_PIECE]; // for individual segment road surfaces
    BYTE sidesColour;
    COORD_3D* coords; // unrotated
    long coordsSize;  // allocated memory size for coords
    /* following not needed since project converted to use Direct3D
    COORD_3D *transformed_coords;
    COORD_2D *screen_coords;
    */
} TRACK_PIECE;

/*    ============================== */
/*    External function declarations */
/*    ============================== */
extern WCHAR* GetTrackName(long track);
extern TrackPack GetTrackPack(void);
extern long GetTrackPackTrackCount(TrackPack pack);
extern long SetTrackPack(TrackPack pack);
extern const WCHAR* GetTrackPackName(void);
/** Loops pack: fire/boost does not drain (steep spiral layouts). */
extern long IsUnlimitedBoostEnabled(void);

/**
 * Amiga Classic-pack division for a track ID (1–4), or 0 if not Classic / unknown.
 * Mapping from reference/StuntCarRacer.s TAB.648c2 + practice menu eori #3.
 */
extern long GetClassicDivisionNumber(long trackId);
/** Classic pack menu order: Division 4→3→2→1 (two tracks each), then other packs. */
extern long GetClassicTrackIdForMenuIndex(long menuIndex);
/** Inverse of GetClassicTrackIdForMenuIndex; returns 0..7. */
extern long GetClassicMenuIndexForTrackId(long trackId);

extern char GetPieceAngleAndTemplate(long piece);

extern long ConvertAmigaTrack(long track);

extern void FreeTrackData(void);

extern HRESULT CreateTrackVertexBuffer(RenderDevice* pDevice);

extern void FreeTrackVertexBuffer(void);

extern void DrawTrack(RenderDevice* pDevice);

extern HRESULT CreateShadowVertexBuffer(RenderDevice* pDevice);

extern void FreeShadowVertexBuffer(void);

extern void RemoveShadowTriangles(void);
extern void StoreShadowTriangle(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, long other_colour);

extern void MoveDrawBridge(void);

extern void ResetDrawBridge(void);

extern HRESULT CreateGroundPlaneVertexBuffer(RenderDevice* pDevice);

extern void FreeGroundPlaneVertexBuffer(void);

extern void DrawGroundPlane(RenderDevice* pDevice);

/* Amiga track trailer (B.1ca2a..f + DAT.1c8a8/c8/e8). Loaded when .bin size > 804. */
extern long TrackDamageLimitStandard;
extern long TrackDamageLimitSuper;
extern long TrackNearStartLinePiece;
extern long TrackHasAmigaTrailer;
extern long TrackSpeedOverlayCount;
extern unsigned char TrackSpeedOverlaySection[];
extern unsigned char TrackSpeedOverlayValue[];
extern long TrackRestartExcludeCount;
extern unsigned char TrackRestartExcludeSection[];

#endif /* _TRACK */
