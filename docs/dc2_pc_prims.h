/*
 * dc2_pc_prims.h -- GPU packet layouts of Dino Crisis 2 PC (Dino2.exe, 2002-02-05).
 *
 * Reconstructed from the field accesses of the packet handlers called by
 * DrawOTag (0x4B50F0, Direct3D path; switch at 0x4B5354) and from the
 * Set* / gte_st* helpers. Layouts are what this binary reads; they differ
 * from the PS1 GPU:
 *   - tag is a full 32-bit pointer (next = tag & 0x00FFFFFF in the walker,
 *     terminator 0xFFFFFFFF); AddPrim sets bit 31 when the hi byte is set;
 *   - vertex positions are floats with a per-vertex z (D3D depth);
 *   - u/v/clut/tpage/colour sit between vertices, so the stride varies.
 * "size" = end of last field read; real allocation may be padded (verify).
 * Ghidra: File > Parse C Source accepts this header as-is.
 */
#ifndef DC2_PC_PRIMS_H
#define DC2_PC_PRIMS_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

/* common 8-byte header; code & 0xFC selects the type,
 * bit 1 = semi-transparent, bit 0 = raw texture (as on PS1) */
typedef struct { u32 tag; u8 r0, g0, b0, code; } PC_P_TAG;

typedef struct { float x, y, z; } PC_XYZ;                               /* 0x0C */
typedef struct { float x, y, z; u8 u, v; u16 clut_or_tpage; } PC_XYZUV;  /* 0x10 */
typedef struct { u8 r, g, b, pad; float x, y, z; } PC_RGBXYZ;           /* 0x10 */

/* 0x20 POLY_F3  size 0x2C                                  handler 0x4B5D00 */
typedef struct { PC_P_TAG h; PC_XYZ v[3]; } PC_POLY_F3;
/* 0x28 POLY_F4  size 0x38                                  handler 0x4B6F50 */
typedef struct { PC_P_TAG h; PC_XYZ v[4]; } PC_POLY_F4;
/* 0x24 POLY_FT3 size 0x38: v[0].clut, v[1].tpage, v[2] unused  0x4B6260 */
typedef struct { PC_P_TAG h; PC_XYZUV v[3]; } PC_POLY_FT3;
/* 0x2C POLY_FT4 size 0x48                                  handler 0x4B70E0 */
typedef struct { PC_P_TAG h; PC_XYZUV v[4]; } PC_POLY_FT4;

/* 0x30 POLY_G3  size 0x34: colour 0 in header, 1..2 before their vertex */
typedef struct { PC_P_TAG h; PC_XYZ v0; PC_RGBXYZ v[2]; } PC_POLY_G3;
/* 0x38 POLY_G4  size 0x44                                  handler 0x4B76F0 */
typedef struct { PC_P_TAG h; PC_XYZ v0; PC_RGBXYZ v[3]; } PC_POLY_G4;

/* 0x34 POLY_GT3 size 0x40, vertex stride 0x14             handler 0x4B74D0 */
typedef struct {
    PC_P_TAG h;
    float x0, y0, z0; u8 u0, v0; u16 clut;      /* +0x08 */
    u8 r1, g1, b1, p1;                           /* +0x18 */
    float x1, y1, z1; u8 u1, v1; u16 tpage;     /* +0x1C */
    u8 r2, g2, b2, p2;                           /* +0x2C */
    float x2, y2, z2; u8 u2, v2; u16 pad2;      /* +0x30 */
} PC_POLY_GT3;
/* 0x3C POLY_GT4 size 0x54                                  handler 0x4B7920 */
typedef struct {
    PC_P_TAG h;
    float x0, y0, z0; u8 u0, v0; u16 clut;      /* +0x08 */
    u8 r1, g1, b1, p1;                           /* +0x18 */
    float x1, y1, z1; u8 u1, v1; u16 tpage;     /* +0x1C */
    u8 r2, g2, b2, p2;                           /* +0x2C */
    float x2, y2, z2; u8 u2, v2; u16 pad2;      /* +0x30 */
    u8 r3, g3, b3, p3;                           /* +0x40 */
    float x3, y3, z3; u8 u3, v3; u16 pad3;      /* +0x44 */
} PC_POLY_GT4;

/* lines (path A ignores them: handler 0x402520 is a bare ret) */
typedef struct { PC_P_TAG h; PC_XYZ v[2]; } PC_LINE_F2;      /* 0x40 size 0x20 */
typedef struct { PC_P_TAG h; PC_XYZ v[3]; } PC_LINE_F3;      /* 0x48 size 0x2C */
typedef struct { PC_P_TAG h; PC_XYZ v[4]; } PC_LINE_F4;      /* 0x4C size 0x38 */
typedef struct { PC_P_TAG h; PC_XYZ v0; PC_RGBXYZ v[1]; } PC_LINE_G2; /* 0x50 size 0x24 */
typedef struct { PC_P_TAG h; PC_XYZ v0; PC_RGBXYZ v[2]; } PC_LINE_G3; /* 0x58 size 0x34 */
typedef struct { PC_P_TAG h; PC_XYZ v0; PC_RGBXYZ v[3]; } PC_LINE_G4; /* 0x5C size 0x44 */

/* rectangles */
typedef struct { PC_P_TAG h; float x, y, z; u16 w, h_; } PC_TILE;          /* 0x60 size 0x18 */
typedef struct { PC_P_TAG h; float x, y, z; } PC_TILE_1;                    /* 0x68 size 0x14 */
typedef struct { PC_P_TAG h; float x, y, z; u8 u, v; u16 clut; u16 w, h_; } PC_SPRT; /* 0x64 size 0x1C */
typedef struct { PC_P_TAG h; float x, y, z; u8 u, v; u16 clut; } PC_SPRT_8; /* 0x74 size 0x18 (also 0x7C SPRT_16) */
/* 0x70 TILE_8 and 0x78 TILE_16 fall to the default case (not drawn) */

/* PC-only packet types */
typedef struct { float x, y; u8 u, v; u16 clut_or_tpage; } PC_XYUV;   /* 0x0C */
/* 0x6C 2D textured quad, no z (screen space)  size 0x38   handler 0x4B8EC0 */
typedef struct { PC_P_TAG h; PC_XYUV v[4]; } PC_POLY_FT4_2D;
/* 0x80 SPRT scaled by depth (w,h / z)  size 0x1C          handler 0x4B8930 */
typedef struct { PC_P_TAG h; float x, y, z; u8 u, v; u16 clut; short w, h_; } PC_SPRT_Z;
/* 0x84 scaled sprite  size 0x20; flags bit 2 = flip (others unknown)  0x4B9400 */
typedef struct { PC_P_TAG h; float x, y, sx, sy; u16 a, b, c; u8 d; u8 flags; } PC_SPRT_SCALED;
/* 0x8C sprite variant  size 0x1C (field meaning unverified)      0x4B9EA0 */
typedef struct { PC_P_TAG h; float x, y; u16 f10; u8 f12, f13; u16 w, h_; u16 f18, f1a; } PC_SPRT_8C;
/* 0x88 marker, handler 0x4BA120 reads nothing */

/* control packets (handled inline in DrawOTag) */
typedef struct { short x, y, w, h; } PC_RECT;
/* 0xE0/0xE8: tpage -> 0x87BAF4, dtd (flags bit0) -> 0x87BAF7,
 * dfe (bit1) -> 0x87BAF6, *tw -> texture window 0x87BAEC (if non-NULL) */
typedef struct { u32 tag; u16 tpage; u8 flags; u8 code; PC_RECT *tw; } PC_DR_MODE;
/* 0xF0: texture window only */
typedef struct { u32 tag; u8 pad[3]; u8 code; PC_RECT *tw; } PC_DR_TWIN;
/* 0xEC: VRAM op 0x4B4A60(&rect, a, b), clipped; 0xFC: MoveImage(&rect, x, y) */
typedef struct { u32 tag; u8 pad[3]; u8 code; PC_RECT rect; int a, b; } PC_DR_VRAM;

#endif
