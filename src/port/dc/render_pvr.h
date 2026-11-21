/* The Dreamcast renderer (render_pvr.c, KOS). */
#ifndef DC2_RENDER_PVR_H
#define DC2_RENDER_PVR_H
#include "dc2/render.h"

/* the PVR set up (640 x 480 video already); NULL on failure */
const Renderer *RenderPvr_Open(void);
/* textures dropped from VRAM for room so far */
u32 RenderPvr_Evicted(void);
/* allocations that failed though enough VRAM was free (in pieces), then retried */
u32 RenderPvr_Fragmented(void);
#endif
