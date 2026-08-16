#ifndef _TRACK_PROPS
#define _TRACK_PROPS

#include "platform_sdl_gl.h"

void RebuildTrackProps(RenderDevice* pDevice);
void FreeTrackPropsVertexBuffer(void);
void DrawTrackProps(RenderDevice* pDevice);

void UpdateTrackLife(RenderDevice* pDevice, float timeSeconds);
void DrawTrackLife(RenderDevice* pDevice);
void FreeTrackLifeVertexBuffer(void);
/** Enhanced-life chase drones: clear then add car targets (player + rivals). Max 2 drones total. */
void ClearTrackLifeCarTargets(void);
void AddTrackLifeCarTarget(float worldX, float worldY, float worldZ, float yawRadians);

void RebuildAsphaltRoadVertexBuffer(RenderDevice* pDevice);
void FreeAsphaltRoadVertexBuffer(void);
void DrawAsphaltRoad(RenderDevice* pDevice);

#endif /* _TRACK_PROPS */
