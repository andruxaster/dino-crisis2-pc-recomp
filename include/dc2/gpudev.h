/* The Direct3D 6 / DirectDraw 4 calls the PC layer's drawing makes, one
 * function each. In-process (the DLL, the harness) they are the COM calls on
 * the objects whose pointers the port keeps in game memory (0x865EBC..); in
 * the standalone builds a renderer implements them (src/pc/gpudev.c). Objects
 * are 32-bit handles either way. Pointer arguments (rects, surface
 * descriptions, colour keys) are host pointers to the D3D/DDraw layouts. */
#ifndef DC2_GPUDEV_H
#define DC2_GPUDEV_H
#include "types.h"

#define A_DDRAW     0x865EBCu      /* IDirectDraw4 */
#define A_PRIMARY   0x865EC0u      /* the front surface */
#define A_BACK      0x865EC4u      /* the back buffer */
#define A_STAGING   0x865ECCu      /* the 256x256 system-memory surface textures are filled through */
#define A_D3D       0x865ED4u      /* IDirect3D3 */
#define A_DEVICE    0x865ED8u      /* IDirect3DDevice3 */
#define A_VIEWPORT  0x865EDCu      /* IDirect3DViewport3 */
#define A_MATERIAL  0x865EE0u      /* IDirect3DMaterial3 (the viewport's background; its handle 0x75DEE8) */

/* the device */
u32 Dev_BeginScene(void);                                           /* +0x24 */
u32 Dev_EndScene(void);                                             /* +0x28 */
u32 Dev_SetRenderState(u32 state, u32 v);                           /* +0x58 */
u32 Dev_DrawPrimitive(u32 type, u32 fvf, gptr verts, u32 n, u32 flags);  /* +0x70 (vertices in game memory) */
u32 Dev_DrawPrimitiveVB(u32 type, u32 vb, u32 start, u32 n, u32 flags);  /* +0x88 */
u32 Dev_SetTexture(u32 stage, u32 tex);                             /* +0x98 */
u32 Dev_SetTextureStageState(u32 stage, u32 type, u32 v);           /* +0xA0 */
/* the viewport and its background material */
u32 Vp_Clear(u32 vp, u32 n, gptr rects, u32 flags);                /* +0x30 */
u32 Vp_SetBackground(u32 vp, u32 mat_handle);                       /* +0x20 */
u32 Mat_SetMaterial(u32 mat, const void *desc);                     /* +0x0C (D3DMATERIAL, 0x50) */
/* DirectDraw */
u32 DD_CreateSurface(void *desc, u32 *surf);                        /* +0x18 (no outer) */
/* Direct3D: vertex buffers (D3DVERTEXBUFFERDESC {size, caps, fvf, count}) */
u32 D3D_CreateVertexBuffer(const void *desc, u32 *vb);              /* +0x24 (flags 4, no outer) */
u32 VB_Lock(u32 vb, u32 flags, void **p);                           /* +0x0C */
u32 VB_Unlock(u32 vb);                                              /* +0x10 */
/* surfaces (and the textures' Release) */
u32 Obj_QueryInterface(u32 o, const void *iid, u32 *out);          /* +0x00 */
u32 Obj_Release(u32 o);                                             /* +0x08 */
u32 Surf_Blt(u32 dst, const void *dr, u32 src, const void *sr, u32 flags, const void *fx); /* +0x14 */
u32 Surf_Flip(u32 s, u32 target, u32 flags);                        /* +0x2C */
u32 Surf_Lock(u32 s, const void *rect, void *desc, u32 flags);      /* +0x64 (no event) */
u32 Surf_Restore(u32 s);                                            /* +0x6C */
u32 Surf_SetColorKey(u32 s, u32 flags, const void *key);            /* +0x74 */
u32 Surf_Unlock(u32 s, const void *rect);                           /* +0x80 */

/* DDSURFACEDESC2 (0x7C bytes): fields used */
#define SD_FLAGS    0x04
#define SD_HEIGHT   0x08
#define SD_WIDTH    0x0C
#define SD_PITCH    0x10
#define SD_SURFACE  0x24           /* (a host pointer) */
#define SD_PIXFMT   0x48           /* DDPIXELFORMAT, 0x20 */
#define SD_CAPS     0x68
#define SD_SIZE     0x7C

#ifndef DC2_IN_PROCESS
/* standalone: the renderer behind these calls (NULL: they do nothing, return 0) */
typedef struct GpuDev GpuDev;
struct GpuDev {
    u32 (*begin_scene)(void), (*end_scene)(void);
    u32 (*set_render_state)(u32 s, u32 v);
    u32 (*draw)(u32 type, u32 fvf, const void *verts, u32 n, u32 flags);
    u32 (*draw_vb)(u32 type, u32 vb, u32 start, u32 n, u32 flags);
    u32 (*set_texture)(u32 stage, u32 tex);
    u32 (*set_tss)(u32 stage, u32 type, u32 v);
    u32 (*vp_clear)(u32 n, const void *rects, u32 flags);
    u32 (*vp_set_background)(u32 mat_handle);
    u32 (*set_material)(u32 mat, const void *desc);
    u32 (*create_surface)(void *desc, u32 *surf);
    u32 (*query_interface)(u32 o, const void *iid, u32 *out);
    u32 (*release)(u32 o);
    u32 (*blt)(u32 dst, const void *dr, u32 src, const void *sr, u32 flags, const void *fx);
    u32 (*flip)(u32 s, u32 target, u32 flags);
    u32 (*lock)(u32 s, const void *rect, void *desc, u32 flags);
    u32 (*restore)(u32 s);
    u32 (*set_color_key)(u32 s, u32 flags, const void *key);
    u32 (*unlock)(u32 s, const void *rect);
    u32 (*create_vb)(const void *desc, u32 *vb);
    u32 (*vb_lock)(u32 vb, void **p);
    u32 (*vb_unlock)(u32 vb);
};
extern const GpuDev *g_gpudev;
#endif
#endif
