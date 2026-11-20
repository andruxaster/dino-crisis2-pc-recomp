/* The standalone renderer: what the PC layer's Direct3D 6 calls come down to
 * (src/pc/gpusoft.c keeps the objects, surfaces and render states; a renderer
 * draws: OpenGL on the PC, the PowerVR2 on the Dreamcast). */
#ifndef DC2_RENDER_H
#define DC2_RENDER_H
#include "types.h"

/* a D3DTLVERTEX (fvf 0x1C4) */
typedef struct {
    float x, y, z, rhw;
    u32 diffuse, specular;          /* a8 r8 g8 b8; the specular's alpha is the vertex fog factor */
    float u, v;
} RVertex;

/* a texture: a surface's pixels (16-bit 1555 or 32-bit 8888, by bpp), a new
   version each time they change */
typedef struct {
    u32 id, version;
    u32 w, h, bpp, pitch;
    const void *pixels;
} RTexture;

/* the render state of a draw, from Direct3D's */
typedef struct {
    u8 blend;                       /* ALPHABLENDENABLE */
    u8 src, dst;                    /* D3DBLEND: 1 zero, 2 one, 3 srccolor, 4 invsrccolor, 5 srcalpha, 6 invsrcalpha */
    u8 alpha_test;                  /* ALPHATESTENABLE, with ref (0..255) and func (D3DCMP) */
    u8 alpha_ref, alpha_func;
    u8 z_test, z_write, z_func;     /* D3DCMP: 1 never, 2 less, 3 equal, 4 lessequal, 5 greater, 6 notequal, 7 greaterequal, 8 always */
    u8 flat;                        /* SHADEMODE flat: the first vertex's colour */
    u8 fog;                         /* FOGENABLE */
    u8 fog_table;                   /* FOGTABLEMODE (0: by vertex, the specular's alpha; 3 linear on depth) */
    u8 modulate2x;                  /* stage 0 colour op MODULATE2X (else MODULATE) */
    u8 filter;                      /* bilinear */
    u32 fog_color;                  /* rgb */
    float fog_start, fog_end;
} RState;

typedef struct Renderer Renderer;
struct Renderer {
    void (*begin_frame)(void);
    void (*clear)(int target, int zbuffer, u32 rgb);
    /* prim: D3DPRIMITIVETYPE (1 points, 2 lines, 3 line strip, 4 triangles, 5 strip, 6 fan) */
    void (*draw)(int prim, const RVertex *v, int n, const RState *st, const RTexture *tex);
    void (*present)(void);
    /* the back buffer (640 x 480) as 16-bit 1555 pixels, for the frame copy */
    void (*read_back)(u16 *dst, u32 w, u32 h, u32 pitch);
    void (*texture_freed)(u32 id);
};

/* the standalone Direct3D: objects made, the game's PC-layer globals set as
   the port's initialisation leaves them (screen w x h), the renderer used */
void PcDraw_InitStandalone(const Renderer *r, u32 screen_w, u32 screen_h);
/* pixels (bpp 2: 1555, 4: 8888 as B, G, R, A bytes) drawn into the back
   buffer over [x0,x1) x [y0,y1), stretched; id and version name the texture */
void PcSoft_DrawImage(u32 id, u32 version, const void *pix, u32 w, u32 h, u32 bpp, s32 x0, s32 y0, s32 x1, s32 y1);
#endif
