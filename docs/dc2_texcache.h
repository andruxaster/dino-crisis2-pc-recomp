/*
 * dc2_texcache.h -- texture cache of Dino Crisis 2 PC (Dino2.exe, 2002-02-05),
 * Direct3D path. Reconstructed from Tex_Bind 0x4B6470 and its callees.
 *
 * Flow for every textured packet:
 *   Tex_Bind(tpage, clut)
 *     page = tpage & 0x1F            (32 pages of 64x256 halfwords)
 *     mode = (tpage >> 7) & 3        (0 = 4bpp, 1 = 8bpp, 2 = 15bpp)
 *     slot = TexCache_Find(page, clut, mode)   key: mode, clut, texture window
 *     miss  -> EndScene, TexCache_Create, BeginScene
 *     dirty -> EndScene, TexCache_Reload,  BeginScene
 *     IDirect3DDevice3::SetTexture(0, entry.tex)
 *
 * Every texture is 256x256 in the device format (16- or 32-bit), cut from
 * VRAM at page origin + texture window offset, converted through a staging
 * surface (Lock -> converter -> Unlock -> Blt into the slot surface).
 * Transparency: CLUT/texel value 0x0000 -> alpha 0 (fully transparent);
 * any other value gets the opaque alpha mask; a non-zero colour that
 * converts to 0 is bumped to 1. The STP bit (bit 15) is not mapped to alpha;
 * semi-transparency comes from the packet code bit 1 and blend states.
 */
#ifndef DC2_TEXCACHE_H
#define DC2_TEXCACHE_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

typedef struct {            /* 24 bytes; g_tex_cache[32 pages][32 slots] @ 0x75E488 */
    u8   state;             /* +0x00  0 free, 1 valid, 2 dirty (reload on next bind) */
    u8   mode;              /* +0x01  0/1/2 = 4/8/15 bpp */
    u16  clut;              /* +0x02  CLUT word from the packet (unused for 15bpp) */
    u32  clut_row_stamp;    /* +0x04  g_vram_row[clut >> 6].stamp at upload time */
    short tw_x, tw_y;       /* +0x08  texture window at upload (0x87BAEC) */
    short tw_w, tw_h;       /* +0x0C */
    void *surface;          /* +0x10  IDirectDrawSurface4* (256x256, DDSCAPS_TEXTURE) */
    void *texture;          /* +0x14  IDirect3DTexture2* passed to SetTexture */
} PC_TEXCACHE_ENTRY;

typedef struct {            /* 8 bytes; g_vram_row[512] @ 0x75CEE8 */
    u32  stamp;             /* +0x00  incremented whenever the row's converted palette is rebuilt */
    void *conv_palette;     /* +0x04  malloc(0x1000): the whole 1024-entry VRAM row in device format,
                                      built lazily by ClutRow_Convert 0x4B4BF0 */
} PC_VRAM_ROW;

/* Converters, table @ 0x73C900 indexed by mode (hand-written asm):
 *   0x4C2A89 4bpp, 0x4C2C2A 8bpp, 0x4C2D5F 15bpp
 *   args: (dst, dst_pitch, src_vram, palette, tw_w, tw_h); 0x800 = VRAM row pitch.
 * Device format globals: 0x87BB42 bits per pixel, 0x87BB43 bytes per pixel,
 * 0x87BB44.. shifts/masks for R,G,B, 0x87BB5C opaque alpha mask. */

#endif
