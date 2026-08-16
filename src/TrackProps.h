#ifndef _TRACK_PROPS
#define _TRACK_PROPS

#include "platform_sdl_gl.h"

void RebuildTrackProps(RenderDevice* pDevice);
void FreeTrackPropsVertexBuffer(void);
void DrawTrackProps(RenderDevice* pDevice);

void UpdateTrackLife(RenderDevice* pDevice, float timeSeconds);
void DrawTrackLife(RenderDevice* pDevice);
void FreeTrackLifeVertexBuffer(void);
/** World-space car anchor for chase drones (Enhanced life). */
void SetTrackLifeCarAnchor(float worldX, float worldY, float worldZ, float yawRadians, bool valid);

void RebuildAsphaltRoadVertexBuffer(RenderDevice* pDevice);
void FreeAsphaltRoadVertexBuffer(void);
void DrawAsphaltRoad(RenderDevice* pDevice);

#endif /* _TRACK_PROPS */
