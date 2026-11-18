/* Models drawn from their vertex blocks (the extra crisis result screen's
 * grade models and the stage pieces): a model {verts, normals, ...; at +0x1C
 * a polygon block {+8 triangles, +0xC quads, u16 +0x10 / +0x12 counts};
 * u8 +0x23 vertex groups of three}. Vertices are projected and lit once
 * into 0x74F5D0 (screen x, y floats), 0x74EE00 (z), 0x74E630 (colours) and
 * 0x750570 (env-map uv from the view-space normals), then the polygons
 * index them. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define SCR_XY   0x74F5D0u
#define SCR_SZ   0x74EE00u
#define SCR_RGB  0x74E630u
#define SCR_UV   0x750570u

static gptr ot_at(u32 z) { return 0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + z); }
static void ot_link(gptr ot, u32 len)
{
    gptr p = U32(0x87DC98);
    U32(p) = U32(ot) | len << 24; U32(ot) = p & 0xFFFFFF;
}
static u32 ot_slot(u32 z) { return (z & ~0xFu) >= 0x3FF0 ? 0x3FF : z >> 4; }
static float depth(u32 z)
{
    s32 sc = (s32)(0x400u << ((U8(G() + 0xB4B) + 1) & 31));
    return (float)((long double)z / (long double)sc);
}

/* the vertices projected and lit; the env-map uv of each normal turned into view space */
void DC2CALL EnvMapDraw(gptr m)                                       /* 0x482940 */
{
    u32 n = U8(m + 0x23);
    gptr v = U32(m), nv = U32(m + 4);
    gptr sxy = SCR_XY, z = SCR_SZ, rgb = SCR_RGB, uv = SCR_UV;
    gptr t = Tmp_Push(8);
    for (; n; n--, v += 0x18, nv += 0x18, sxy += 0x18, z += 0xC, rgb += 0xC) {
        gte_ldv3c(v); gte_rtpt();
        gte_stsxy3c(sxy); gte_stsz3(z);
        gte_ldv3c(nv); gte_ncct();
        for (u32 i = 0; i < 3; i++, uv += 2) {
            gte_ldv0(nv + 8 * i); gte_rtv0(); gte_stsv(t);
            s32 x = S16(t), y = S16(t + 2), zz = S16(t + 4);
            u32 a = (u32)ratan2((float)x, (float)zz) & 0xFFF;
            s16 az = (s16)(zz < 0 ? -zz : zz);
            U16(t + 4) = (u16)az;
            u32 b = (u32)ratan2((float)az, (float)y) & 0xFFF;
            U8(uv) = (u8)(a >> 4);
            U8(uv + 1) = (u8)(b >> 3);
        }
        gte_strgb3(rgb);
    }
    Tmp_Pop(8);
}

/* the block's quads with the env-map uv: {u16 tpage, u16 clut, u32 colour, u8 mode};
   mode 1: semi-transparent, mode 2: the lit colours (else the colour). The
   vertex buffers: screen xy, depth (or the order-table slots: slots), colours, uv */
static void env_quads(gptr b, gptr p, gptr xy, gptr sz, gptr rgb, gptr uv, int slots)
{
    gptr q = U32(b + 0xC);
    u32 n = U16(b + 0x12);
    u16 tpage = U16(p), clut = U16(p + 2);
    u32 mode = U8(p + 8), col = U32(p + 4);
    u8 code = (u8)(((mode & 1) | 0x1E) << 1);
    gptr t = Tmp_Push(4);
    for (; n; n--, q += 0x10) {
        u32 i[4] = {U16(q), U16(q + 2), U16(q + 4), U16(q + 6)};
        gte_ldsxy3(xy + 8 * i[0], xy + 8 * i[1], xy + 8 * i[2]);
        gte_nclip(); gte_stopz(t);
        if (S32(t) < 0) continue;
        gptr pr = U32(0x87DC98);
        for (u32 k = 0; k < 4; k++) {
            U32(pr + 8 + 0x14 * k) = U32(xy + 8 * i[k]);
            U32(pr + 0xC + 0x14 * k) = U32(xy + 8 * i[k] + 4);
        }
        for (u32 k = 0; k < 4; k++) U32(pr + 4 + 0x14 * k) = mode & 2 ? U32(rgb + 4 * i[k]) : col;
        for (u32 k = 0; k < 4; k++) U16(pr + 0x14 + 0x14 * k) = U16(uv + 2 * i[k]);
        U16(pr + 0x2A) = tpage; U16(pr + 0x16) = clut; U8(pr + 7) = code;
        for (u32 k = 0; k < 4; k++) F32(pr + 0x10 + 0x14 * k) = depth(U32(sz + 4 * i[k]));
        u32 z = U32(sz + 4 * i[0]);
        for (u32 k = 1; k < 4; k++) if (z < U32(sz + 4 * i[k])) z = U32(sz + 4 * i[k]);
        ot_link(ot_at(slots ? z : ot_slot(z)), 0xC);
        U32(0x87DC98) += 0x54;
    }
    Tmp_Pop(4);
}

/* the same for the triangles */
static void env_tris(gptr b, gptr p, gptr xy, gptr sz, gptr rgb, gptr uv, int slots)
{
    gptr q = U32(b + 8);
    u32 n = U16(b + 0x10);
    u16 tpage = U16(p), clut = U16(p + 2);
    u32 mode = U8(p + 8), col = U32(p + 4);
    u8 code = (u8)(((mode & 1) | 0x1A) << 1);
    gptr t = Tmp_Push(4);
    for (; n; n--, q += 0xC) {
        u32 i[3] = {U16(q), U16(q + 2), U16(q + 4)};
        gte_ldsxy3(xy + 8 * i[0], xy + 8 * i[1], xy + 8 * i[2]);
        gte_nclip(); gte_stopz(t);
        if (S32(t) < 0) continue;
        gptr pr = U32(0x87DC98);
        for (u32 k = 0; k < 3; k++) {
            U32(pr + 8 + 0x14 * k) = U32(xy + 8 * i[k]);
            U32(pr + 0xC + 0x14 * k) = U32(xy + 8 * i[k] + 4);
        }
        for (u32 k = 0; k < 3; k++) U32(pr + 4 + 0x14 * k) = mode & 2 ? U32(rgb + 4 * i[k]) : col;
        for (u32 k = 0; k < 3; k++) U16(pr + 0x14 + 0x14 * k) = U16(uv + 2 * i[k]);
        U16(pr + 0x2A) = tpage; U16(pr + 0x16) = clut; U8(pr + 7) = code;
        for (u32 k = 0; k < 3; k++) F32(pr + 0x10 + 0x14 * k) = depth(U32(sz + 4 * i[k]));
        u32 z = U32(sz + 4 * i[0]);
        for (u32 k = 1; k < 3; k++) if (z < U32(sz + 4 * i[k])) z = U32(sz + 4 * i[k]);
        ot_link(ot_at(slots ? z : ot_slot(z)), 9);
        U32(0x87DC98) += 0x40;
    }
    Tmp_Pop(4);
}

void DC2CALL Model_EnvQuads(gptr b, gptr p)                           /* 0x482BD0 */
{
    env_quads(b, p, SCR_XY, SCR_SZ, SCR_RGB, SCR_UV, 0);
}

void DC2CALL Model_EnvTris(gptr b, gptr p)                            /* 0x482F80 */
{
    env_tris(b, p, SCR_XY, SCR_SZ, SCR_RGB, SCR_UV, 0);
}

void DC2CALL Model_SetTexture(gptr m, gptr p)                         /* 0x483290 */
{
    Model_EnvQuads(m + 0x1C, p);
    Model_EnvTris(m + 0x1C, p);
}

/* textured, lit quads {4 u16 vertex indices, uv0..uv3}: each projected here */
void DC2CALL MeshDrawGT4(gptr v, gptr nv, gptr b, u32 tpage, u32 clut)   /* 0x4832C0 */
{
    gptr q = U32(b + 0xC);
    u32 n = U16(b + 0x12);
    gptr L = Tmp_Push(0x18), z = L + 8;
    for (; n; n--, q += 0x10) {
        u32 i0 = U16(q), i1 = U16(q + 2), i2 = U16(q + 4), i3 = U16(q + 6);
        gte_ldv3(v + 8 * i0, v + 8 * i1, v + 8 * i2); gte_rtpt();
        gptr p = U32(0x87DC98);
        gte_stsxy3_gt3(p); gte_stsz3(z);
        gte_nclip(); gte_stopz(L);
        if (S32(L) < 0) continue;
        gte_ldv3(nv + 8 * i0, nv + 8 * i1, nv + 8 * i2); gte_ncct();
        gte_strgb3_gt3(U32(0x87DC98));
        gte_ldv0(v + 8 * i3); gte_rtps();
        gte_stsxy(U32(0x87DC98) + 0x44); gte_stsz(z + 0xC);
        gte_ldv0(nv + 8 * i3); gte_nccs();
        gte_strgb(U32(0x87DC98) + 0x40);
        PC_stsz4_gt4(U32(0x87DC98));
        p = U32(0x87DC98);
        U8(p + 0x14) = U8(q + 8); U8(p + 0x15) = U8(q + 9);
        U8(p + 0x28) = U8(q + 0xA); U8(p + 0x29) = U8(q + 0xB);
        U8(p + 0x3C) = U8(q + 0xC); U8(p + 0x3D) = U8(q + 0xD);
        U8(p + 0x50) = U8(q + 0xE); U8(p + 0x51) = U8(q + 0xF);
        U16(p + 0x2A) = (u16)tpage; U16(p + 0x16) = (u16)clut; U8(p + 7) = 0x3C;
        u32 zm = U32(z);
        for (u32 k = 1; k < 4; k++) if (zm < U32(z + 4 * k)) zm = U32(z + 4 * k);
        ot_link(ot_at(ot_slot(zm)), 0xC);
        U32(0x87DC98) += 0x54;
    }
    Tmp_Pop(0x18);
}

/* textured, lit triangles {3 u16 vertex indices, uv2, uv0, uv1} */
void DC2CALL MeshDrawGT3(gptr v, gptr nv, gptr b, u32 tpage, u32 clut)   /* 0x4834F0 */
{
    gptr q = U32(b + 8);
    u32 n = U16(b + 0x10);
    gptr L = Tmp_Push(0x10), z = L + 4;
    for (; n; n--, q += 0xC) {
        u32 i0 = U16(q), i1 = U16(q + 2), i2 = U16(q + 4);
        gte_ldv3(v + 8 * i0, v + 8 * i1, v + 8 * i2); gte_rtpt();
        gte_stsxy3_gt3(U32(0x87DC98)); gte_stsz3(z);
        gte_nclip(); gte_stopz(L);
        if (S32(L) < 0) continue;
        gte_ldv3(nv + 8 * i0, nv + 8 * i1, nv + 8 * i2); gte_ncct();
        gte_strgb3_gt3(U32(0x87DC98));
        PC_stsz3_gt3(U32(0x87DC98));
        gptr p = U32(0x87DC98);
        U8(p + 0x14) = U8(q + 8); U8(p + 0x15) = U8(q + 9);
        U8(p + 0x28) = U8(q + 0xA); U8(p + 0x29) = U8(q + 0xB);
        U8(p + 0x3C) = U8(q + 6); U8(p + 0x3D) = U8(q + 7);
        U16(p + 0x2A) = (u16)tpage; U16(p + 0x16) = (u16)clut; U8(p + 7) = 0x34;
        u32 zm = U32(z);
        for (u32 k = 1; k < 3; k++) if (zm < U32(z + 4 * k)) zm = U32(z + 4 * k);
        ot_link(ot_at(ot_slot(zm)), 9);
        U32(0x87DC98) += 0x40;
    }
    Tmp_Pop(0x10);
}

void DC2CALL Model_DrawTex(gptr m, u32 tpage, u32 clut)               /* 0x4836A0 */
{
    MeshDrawGT4(U32(m), U32(m + 4), m + 0x1C, tpage, clut);
    MeshDrawGT3(U32(m), U32(m + 4), m + 0x1C, tpage, clut);
}

/* ---- the ending's three 3D scenes (tasks; each a mode byte at T+0 and a
   table of modes): vertices projected once into per-scene buffers with the
   order-table slot of each vertex (its depth / 16), then the polygons ---- */

/* the vertices: screen xy, slots; lit into rgb (with normals nv) unless 0;
   env-map uv (from the view-space normals) unless 0, u offset by U8 0x7519C8 */
static void proj_verts(u32 n, gptr v, gptr nv, gptr xy, gptr slots, gptr rgb, gptr uv, int uoff)
{
    gptr t = Tmp_Push(0x14), z = t + 8;
    for (; n; n--, v += 0x18, nv += 0x18, xy += 0x18, slots += 0xC) {
        gte_ldv3c(v); gte_rtpt();
        gte_stsxy3c(xy); gte_stsz3(z);
        if (rgb) { gte_ldv3c(nv); gte_ncct(); }
        for (u32 k = 0; k < 3; k++) U32(slots + 4 * k) = ot_slot(U32(z + 4 * k));
        if (uv)
            for (u32 i = 0; i < 3; i++, uv += 2) {
                gte_ldv0(nv + 8 * i); gte_rtv0(); gte_stsv(t);
                s32 x = S16(t), y = S16(t + 2), zz = S16(t + 4);
                u32 a = (u32)ratan2((float)x, (float)zz) & 0xFFF;
                s16 az = (s16)(zz < 0 ? -zz : zz);
                u32 b = (u32)ratan2((float)az, (float)y) & 0xFFF;
                U8(uv + 1) = (u8)(b >> 3);
                U8(uv) = (u8)((a >> 4) + (uoff ? U8(0x7519C8) : 0));
            }
        if (rgb) { gte_strgb3(rgb); rgb += 0xC; }
    }
    Tmp_Pop(0x14);
}

/* gouraud quads / triangles by the slots, back faces dropped (cull) or not */
static void shade_quads(gptr b, gptr xy, gptr slots, gptr rgb, int cull, u8 code)
{
    gptr q = U32(b + 0xC);
    u32 n = U16(b + 0x12);
    gptr t = Tmp_Push(4);
    for (; n; n--, q += 0x10) {
        u32 i[4] = {U16(q), U16(q + 2), U16(q + 4), U16(q + 6)};
        if (cull) {
            gte_ldsxy3(xy + 8 * i[0], xy + 8 * i[1], xy + 8 * i[2]);
            gte_nclip(); gte_stopz(t);
            if (S32(t) < 0) continue;
        }
        gptr pr = U32(0x87DC98);
        for (u32 k = 0; k < 4; k++) {
            U32(pr + 8 + 0x10 * k) = U32(xy + 8 * i[k]);
            U32(pr + 0xC + 0x10 * k) = U32(xy + 8 * i[k] + 4);
        }
        for (u32 k = 0; k < 4; k++) U32(pr + 4 + 0x10 * k) = U32(rgb + 4 * i[k]);
        U8(pr + 7) = code;
        for (u32 k = 0; k < 4; k++) F32(pr + 0x10 + 0x10 * k) = depth(U32(slots + 4 * i[k]));
        u32 z = U32(slots + 4 * i[0]);
        for (u32 k = 1; k < 4; k++) if (z < U32(slots + 4 * i[k])) z = U32(slots + 4 * i[k]);
        ot_link(ot_at(z), 8);
        U32(0x87DC98) += 0x44;
    }
    Tmp_Pop(4);
}

static void shade_tris(gptr b, gptr xy, gptr slots, gptr rgb, int cull, u8 code)
{
    gptr q = U32(b + 8);
    u32 n = U16(b + 0x10);
    gptr t = Tmp_Push(4);
    for (; n; n--, q += 0xC) {
        u32 i[3] = {U16(q), U16(q + 2), U16(q + 4)};
        if (cull) {
            gte_ldsxy3(xy + 8 * i[0], xy + 8 * i[1], xy + 8 * i[2]);
            gte_nclip(); gte_stopz(t);
            if (S32(t) < 0) continue;
        }
        gptr pr = U32(0x87DC98);
        for (u32 k = 0; k < 3; k++) {
            U32(pr + 8 + 0x10 * k) = U32(xy + 8 * i[k]);
            U32(pr + 0xC + 0x10 * k) = U32(xy + 8 * i[k] + 4);
        }
        for (u32 k = 0; k < 3; k++) U32(pr + 4 + 0x10 * k) = U32(rgb + 4 * i[k]);
        U8(pr + 7) = code;
        for (u32 k = 0; k < 3; k++) F32(pr + 0x10 + 0x10 * k) = depth(U32(slots + 4 * i[k]));
        u32 z = U32(slots + 4 * i[0]);
        for (u32 k = 1; k < 3; k++) if (z < U32(slots + 4 * i[k])) z = U32(slots + 4 * i[k]);
        ot_link(ot_at(z), 6);
        U32(0x87DC98) += 0x34;
    }
    Tmp_Pop(4);
}

/* a 2D sprite (code 0x64) at order-table slot `slot`; a draw mode packet */
static void sprite(u32 slot, u32 x, u32 y, u8 u, u8 v, u16 w, u16 h, u16 clut)
{
    gptr p = U32(0x87DC98);
    U32(p + 8) = x; U32(p + 0xC) = y; U32(p + 0x10) = 0x3C23D70A;     /* (floats; z 0.01) */
    U8(p + 0x14) = u; U8(p + 0x15) = v;
    U16(p + 0x18) = w; U16(p + 0x1A) = h; U16(p + 0x16) = clut;
    U32(p + 4) = 0x64808080;
    ot_link(ot_at(slot), 4);
    U32(0x87DC98) += 0x1C;
}
static void draw_mode(u32 slot, u32 mode)
{
    gptr p = U32(0x87DC98);
    U32(p + 4) = mode; U32(p + 8) = 0;
    ot_link(ot_at(slot), 2);
    U32(0x87DC98) += 0xC;
}

/* the camera {rotation, translation; +0x20 angles, +0x28 target, +0x30 offset,
   +0x34 distance, +0x38 eye} pointed at its target */
static void cam_look(gptr c)
{
    gptr a = Tmp_Push(8);
    Angles_LookAt(c + 0x28, c + 0x38, c + 0x20);
    S16(a) = (s16)-S16(c + 0x20); S16(a + 2) = (s16)-S16(c + 0x22);
    U16(c + 0x24) = 0; U16(a + 4) = 0;
    RotMatrixZXY(a, c);
    U16(c + 0x32) = 0; U16(c + 0x30) = 0;
    U16(c + 0x34) = (u16)Camera_Distance(c + 0x38, c + 0x28);
    Tmp_Pop(8);
}

/* m = camera * m, its translation the camera's view of pos (+ the camera's offset) */
static void cam_view(gptr c, gptr m, s32 x, s32 y, s32 z)
{
    gptr v = Tmp_Push(0xC);
    MulMatrix0(c, m, m);
    S32(v) = x - S16(c + 0x28); S32(v + 4) = y - S16(c + 0x2A); S32(v + 8) = z - S16(c + 0x2C);
    ApplyMatrixLV(c, v, m + 0x14);
    S32(m + 0x14) += S16(c + 0x30); S32(m + 0x18) += S16(c + 0x32); S32(m + 0x1C) += U16(c + 0x34);
    Tmp_Pop(0xC);
}

/* the light matrix: the three light vectors at lv turned by the transpose of m */
static void lights(gptr m, gptr lv, gptr L)
{
    TransposeMatrix(m, L);
    SetRotMatrix(L);
    gte_ldv3c(lv);
    gte_rtv0(); gte_stsv(L);
    gte_rtv1(); gte_stsv(L + 6);
    gte_rtv2(); gte_stsv(L + 0xC);
}

/* scene 1 (T 0x750958, modes 0x738A64) */
#define E1_XY    0x7519D0u
#define E1_SLOTS 0x7511F8u
#define E1_RGB   0x750A28u
#define E1_UV    0x752970u

void DC2CALL End1_Verts(gptr m)                                          /* 0x483F40 */
{
    proj_verts(U8(m + 0x23), U32(m), U32(m + 4), E1_XY, E1_SLOTS, E1_RGB, E1_UV, 1);
}
void DC2CALL End1_TexQuads(gptr b, gptr p)                               /* 0x484270 */
{
    env_quads(b, p, E1_XY, E1_SLOTS, E1_RGB, E1_UV, 1);
}
void DC2CALL End1_TexTris(gptr b, gptr p)                                /* 0x484600 */
{
    env_tris(b, p, E1_XY, E1_SLOTS, E1_RGB, E1_UV, 1);
}
void DC2CALL End1_Tex(gptr m, gptr p)                                    /* 0x484240 */
{
    End1_TexQuads(m + 0x1C, p);
    End1_TexTris(m + 0x1C, p);
}
void DC2CALL End1_ShadeQuads(gptr b)                                     /* 0x484910 */
{
    shade_quads(b, E1_XY, E1_SLOTS, E1_RGB, 1, 0x38);
}
void DC2CALL End1_ShadeTris(gptr b)                                      /* 0x484BD0 */
{
    shade_tris(b, E1_XY, E1_SLOTS, E1_RGB, 1, 0x30);
}
void DC2CALL End1_Shade(gptr m)                                          /* 0x4848F0 */
{
    End1_ShadeQuads(m + 0x1C);
    End1_ShadeTris(m + 0x1C);
}

void DC2CALL End1_Init(gptr t)                                           /* 0x483720 */
{
    U8(t)++;
    gptr c = t + 0x3C;
    U16(c + 0x38) = 0xF99F; U16(c + 0x3A) = 0xFC18; U16(c + 0x3C) = 0xF5EC;
    U16(c + 0x28) = 0xFE7A; U16(c + 0x2A) = 0xFD99; U16(c + 0x2C) = 0xFF17;
    U16(t + 0xCA) = 0x1F3;
    cam_look(c);
    U16(t + 4) = 0xF060; U16(t + 6) = 0xF060; U16(t + 8) = 0xFF38;      /* the light, the colour matrix */
    U16(t + 0xC) = 0; U16(t + 0xE) = 0; U16(t + 0x10) = 0;
    U16(t + 0x14) = 0; U16(t + 0x16) = 0; U16(t + 0x18) = 0;
    U16(t + 0x1C) = 0x1000; U16(t + 0x22) = 0; U16(t + 0x28) = 0;
    U16(t + 0x1E) = 0x1000; U16(t + 0x24) = 0x1000; U16(t + 0x2A) = 0;
    U16(t + 0x20) = 0; U16(t + 0x26) = 0; U16(t + 0x2C) = 0x1000;
}

/* the sky: two sprites at the back */
void DC2CALL End1_Sky(gptr t)                                            /* 0x483820 */
{
    sprite(0x3FE, 0, 0x42400000, 0, 0x10, 0xB0, 0x60, 0x7F80);          /* (0, 48) */
    sprite(0x3FE, 0x43300000, 0x42400000, 0, 0x80, 0x90, 0x60, 0x7F80); /* (176, 48) */
    draw_mode(0x3FE, 0xE100049E);
}

/* the ground: rows of 16x16 sprites scrolled by T+0xC8 */
void DC2CALL End1_Ground(gptr t)                                         /* 0x483990 */
{
    U16(t + 0xC8) = (u16)((U16(t + 0xC8) - 1) & 0x1FF);
    for (u32 r = 0; r < 0x78; r += 0x10) {
        float y = (float)r;
        u32 d = U16(t + 0xC8);
        u32 x = (u32)-(s32)(d & 0xF);
        d = d >> 4 << 4;
        for (u32 k = 0; k < 0x15; k++, x += 0x10) {
            gptr p = U32(0x87DC98);
            F32(p + 8) = (float)(long double)x;          /* (the first column: unsigned, off screen when scrolled) */
            F32(p + 0xC) = y; U32(p + 0x10) = 0x3C23D70A;
            U8(p + 0x14) = (u8)d;
            U8(p + 0x15) = (u8)(d >= 0x100 ? r + 0x80 : r);
            d = (d + 0x10) & 0x1FF;
            U16(p + 0x16) = 0x7F40; U32(p + 4) = 0x7C808080;
            ot_link(ot_at(0x3FF), 3);
            U32(0x87DC98) += 0x18;
        }
    }
    draw_mode(0x3FF, 0xE100049C);
}

/* the backdrop: two sprites near the front */
void DC2CALL End1_Backdrop(gptr t)                                       /* 0x483DD0 */
{
    sprite(0xA, 0, 0x42000000, 0, 0, 0xA0, 0x80, 0x7F00);               /* (0, 32) */
    sprite(0xA, 0x43200000, 0x42400000, 0, 0xA0, 0xA0, 0x60, 0x7F00);   /* (160, 48) */
    draw_mode(0xA, 0xE100049A);
}

static void tex_par(gptr p, u16 tpage, u16 clut, u32 col, u8 mode)
{
    U16(p) = tpage; U16(p + 2) = clut; U8(p + 8) = mode; U32(p + 4) = col;
}

void DC2CALL End1_Run(gptr t)                                            /* 0x483B00 */
{
    gptr L = Tmp_Push(0x70), col = L, P = L + 4, M3 = L + 0x10, M2 = L + 0x30, M1 = L + 0x50;
    RotMatrix(0x752D58, M1);
    SetBackColor(0x20, 0x20, 0x20);
    SetGeomScreen(U16(t + 0xCA));
    SetGeomOffset(0xA0, 0x78);
    gptr c = t + 0x3C, v = L + 0x10;
    MulMatrix0(c, M1, M2);
    S32(v) = -S16(c + 0x28); S32(v + 4) = -S16(c + 0x2A); S32(v + 8) = -S16(c + 0x2C);
    ApplyMatrixLV(c, v, M2 + 0x14);
    S32(M2 + 0x14) += S16(c + 0x30); S32(M2 + 0x18) += S16(c + 0x32); S32(M2 + 0x1C) += U16(c + 0x34);
    lights(M1, t + 4, M3);
    SetLightMatrix(M3);
    SetColorMatrix(t + 0x1C);
    SetTransMatrix(M2);
    SetRotMatrix(M2);
    U32(0x7519C8) = (U32(0x7519C8) + 1) & 0x7F;
    U32(col) = 0x808080; gte_ldrgb(col);
    End1_Verts(0x625800); tex_par(P, 0xAE, 0x7FC0, 0x202020, 1); End1_Tex(0x625800, P); End1_Shade(0x625800);
    End1_Verts(0x629D94); tex_par(P, 0xAE, 0x7FC0, 0x202020, 1); End1_Tex(0x629D94, P); End1_Shade(0x629D94);
    U32(col) = 0x606060; gte_ldrgb(col);
    End1_Verts(0x62963C); End1_Shade(0x62963C);
    End1_Verts(0x62995C); End1_Shade(0x62995C);
    U32(col) = 0x808080; gte_ldrgb(col);
    End1_Verts(0x628994); tex_par(P, 0xAE, 0x7FC0, 0x202020, 1); End1_Tex(0x628994, P);
    End1_Verts(0x62B570); tex_par(P, 0xAE, 0x7FC0, 0x202020, 1); End1_Tex(0x62B570, P);
    End1_Backdrop(t);
    End1_Sky(t);
    End1_Ground(t);
    u32 k = U32(0x87DE0C) << 12;
    Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
    Tmp_Pop(0x70);
}

void DC2CALL End1Task(void)                                              /* 0x4836E0 */
{
    memset(MEM(0x750958), 0, 0x34 * 4);
    for (;;) {
        switch (U8(0x750958)) {
        case 0: End1_Init(0x750958); break;
        case 1: End1_Run(0x750958); break;
        default: Orig_Call1(U32(0x738A64 + U8(0x750958) * 4), 0x750958); break;
        }
        Task_Sleep(1);
    }
}

/* scene 2 (T 0x752D60, modes 0x738A6C): two models on twelve paths of
   0x4B0 positions {s16 x, y, z} each (T+4: {pos, path, on} x 12) */
#define E2_XY    0x753E60u
#define E2_SLOTS 0x753690u
#define E2_RGB   0x752EC0u
#define E2_UV    0x754E00u

void DC2CALL End2_Verts(gptr m)                                          /* 0x4851E0 */
{
    proj_verts(U8(m + 0x23), U32(m), U32(m + 4), E2_XY, E2_SLOTS, 0, E2_UV, 0);
}
void DC2CALL End2_TexQuads(gptr b, gptr p)                               /* 0x4854F0 */
{
    env_quads(b, p, E2_XY, E2_SLOTS, E2_RGB, E2_UV, 1);
}
void DC2CALL End2_TexTris(gptr b, gptr p)                                /* 0x485880 */
{
    env_tris(b, p, E2_XY, E2_SLOTS, E2_RGB, E2_UV, 1);
}
void DC2CALL End2_Tex(gptr m, gptr p)                                    /* 0x4854B0 */
{
    gptr c = Tmp_Push(4);
    U32(c) = 0x80; gte_ldrgb(c);                       /* (the colour: in its own argument's place) */
    End2_TexQuads(m + 0x1C, p);
    End2_TexTris(m + 0x1C, p);
    Tmp_Pop(4);
}

static void path_pos(gptr r, u32 i)
{
    gptr s = U32(r + 8) + 6 * i;
    U16(r) = U16(s); U16(r + 2) = (u16)-S16(s + 2); U16(r + 4) = (u16)-S16(s + 4);
}

void DC2CALL End2_Init(gptr t)                                           /* 0x484E40 */
{
    static const u32 path[12] = {0x600900, 0x602520, 0x604140, 0x605D60, 0x607980, 0x6095A0,
                                 0x60B1C0, 0x60CDE0, 0x60EA00, 0x610620, 0x612240, 0x613E60};
    U8(t)++;
    Flag_Set(3, 0, 1);
    U32(G() + 8) = 0x6C1500;
    gptr c = t + 0xC4;
    U16(c + 0x2A) = 0; U16(c + 0x2C) = 0;
    U16(c + 0x38) = 0; U16(c + 0x3A) = 0; U16(c + 0x3C) = 0xC568;
    U16(c + 0x28) = 0xA;
    U16(t + 0x150) = 0x1F3;
    cam_look(c);
    U32(t + 0x154) = 0;
    for (u32 k = 0; k < 12; k++) {
        gptr r = t + 4 + 0x10 * k;
        U32(r + 8) = path[k];
        path_pos(r, U32(t + 0x154));
        U32(r + 0xC) = 1;
    }
}

void DC2CALL End2_Draw(gptr t)                                           /* 0x484FB0 */
{
    gptr B = Tmp_Push(0x3C), P = B, L = B + 0x1C;
    U16(0x752EBC) += 0x100; U16(0x752EBA) = 0; U16(0x752EB8) = 0;
    RotMatrix(0x752EB8, L);
    u32 n = U32(t + 0x154) + 1;
    U32(t + 0x154) = n;
    if (n >= 0x4B0) U32(t + 0x154) = 0;
    gptr c = t + 0xC4;
    for (u32 k = 0; k < 12; k++) {
        gptr r = t + 4 + 0x10 * k;
        path_pos(r, U32(t + 0x154));
        if (!U32(r + 0xC)) continue;
        cam_view(c, L, S16(r), S16(r + 2), S16(r + 4));      /* (L turned again for each one) */
        SetRotMatrix(L);
        SetTransMatrix(L);
        End2_Verts(0x600480); tex_par(P, 0xAE, 0x7FC0, 0x808080, 1); End2_Tex(0x600480, P);
        End2_Verts(0x600000); tex_par(P, 0x9E, 0x7F80, 0xA0A0A0, 1); End2_Tex(0x600000, P);
    }
    Tmp_Pop(0x3C);
}

void DC2CALL End2_Run(gptr t)                                            /* 0x485160 */
{
    gptr a = Tmp_Push(0x28), m = a + 8;
    U16(a + 4) = 0; U16(a + 2) = 0; U16(a) = 0;
    RotMatrix(a, m);                                    /* (unused) */
    SetGeomScreen(U16(t + 0x150));
    SetGeomOffset(0xA0, 0x78);
    End2_Draw(t);
    u32 k = U32(0x87DE0C) << 12;
    Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
    Tmp_Pop(0x28);
}

void DC2CALL End2Task(void)                                              /* 0x484E00 */
{
    memset(MEM(0x752D60), 0, 0x56 * 4);
    for (;;) {
        switch (U8(0x752D60)) {
        case 0: End2_Init(0x752D60); break;
        case 1: End2_Run(0x752D60); break;
        default: Orig_Call1(U32(0x738A6C + U8(0x752D60) * 4), 0x752D60); break;
        }
        Task_Sleep(1);
    }
}

/* scene 3 (T 0x7551E8, modes 0x738B18): a model whose vertices and normals
   are interpolated (Hermite) between 20 key frames of 15 steps */
#define E3_VERT  0x757208u
#define E3_NORM  0x7552C8u
#define E3_XY    0x7581A8u
#define E3_SLOTS 0x756A38u
#define E3_RGB   0x756268u

void DC2CALL End3_Verts(gptr m)                                          /* 0x486320 */
{
    proj_verts(U8(m + 0x23), E3_VERT, E3_NORM, E3_XY, E3_SLOTS, E3_RGB, 0, 0);
}
void DC2CALL End3_ShadeQuads(gptr b)                                     /* 0x486400 */
{
    shade_quads(b, E3_XY, E3_SLOTS, E3_RGB, 0, 0x3A);
}
void DC2CALL End3_ShadeTris(gptr b)                                      /* 0x486680 */
{
    shade_tris(b, E3_XY, E3_SLOTS, E3_RGB, 0, 0x32);
}
void DC2CALL End3_Shade(gptr m)                                          /* 0x485F00 */
{
    End3_ShadeQuads(m + 0x1C);
    End3_ShadeTris(m + 0x1C);
}

static s32 mul(s32 a, s32 b) { return (s32)((u32)a * (u32)b); }

/* the vertices (which 0) or normals (1) of step T+4 between key frames T+5 and T+5+1
   (table 0x738A78: {vertices, normals} x 20); T+0xCC vectors */
void DC2CALL End3_Interp(gptr t, u32 which)                              /* 0x485F20 */
{
    s32 f = (s8)U8(t + 5);
    s32 f0 = f - 1, f2 = f + 1, f3 = f + 2;
    if (f0 < 0) f0 = 0x13;
    if (f2 >= 0x14) f2 -= 0x14;
    if (f3 >= 0x14) f3 -= 0x14;
    gptr tab = 0x738A78 + (which ? 4 : 0);
    gptr p0 = U32(tab + 8 * f0), p1 = U32(tab + 8 * f), p2 = U32(tab + 8 * f2), p3 = U32(tab + 8 * f3);
    gptr out = which ? 0x7552C8 : 0x757208;
    s32 u = (s32)(U8(t + 4) << 14) / 15, u2 = mul(u, u) >> 14, u3 = mul(u2, u) >> 14;
    for (u32 n = U32(t + 0xCC); n; n--, out += 8, p0 += 2, p1 += 2, p2 += 2, p3 += 2)
        for (u32 c = 0; c < 3; c++, p0 += 2, p1 += 2, p2 += 2, p3 += 2) {
            s32 v1 = S16(p1), v2 = S16(p2), v3 = S16(p3);
            s32 m1 = U8(t + 4) == 0 ? v2 - v1 : mul(v2 - S16(p0), 15) / 30;
            s32 m2 = U8(t + 4) == 0xE ? v3 - v2 : mul(v3 - v1, 15) / 30;
            s32 a = mul(3, v2 - v1) - 2 * m1 - m2, b = m2 + 2 * (v1 - v2) + m1;
            s32 r = mul(a, u2) + mul(b, u3) + mul(m1, u);
            U16(out + 2 * c) = (u16)((r >> 14) + v1);
        }
}

/* the step: the next of 15 between two key frames, 20 key frames round */
void DC2CALL End3_Anim(gptr t)                                           /* 0x485D00 */
{
    U32(t + 0xCC) = U8(0x600023) * 3;
    End3_Interp(t, 0);
    End3_Interp(t, 1);
    if (++U8(t + 4) >= 0xF) {
        s32 f = (s8)U8(t + 5) + 1;
        U8(t + 4) = 0;
        U8(t + 5) = (u8)((u32)f < 0x14 ? f : 0);
    }
}

/* the logo: one of two sprites (T+6) */
void DC2CALL End3_Logo(gptr t)                                           /* 0x486170 */
{
    if (!U8(t + 6)) {
        sprite(0x3FF, 0x434B0000, 0x42480000, 0, 0, 0x28, 0x88, 0x7F80);   /* (203, 50) */
        draw_mode(0x3FF, 0xE100049E);
    } else {
        sprite(0x1E, 0x43360000, 0x41880000, 0, 0, 0x80, 0x100, 0x7FC0);   /* (182, 17) */
        draw_mode(0x1E, 0xE100048E);
    }
}

void DC2CALL End3_Init(gptr t)                                           /* 0x485BB0 */
{
    U8(t)++;
    gptr c = t + 0x40;
    U16(c + 0x38) = 0x1E; U16(c + 0x3A) = 0xFBAA; U16(c + 0x3C) = 0xEC78;
    U16(c + 0x28) = 0xC80; U16(c + 0x2A) = 0xFC90; U16(c + 0x2C) = 0;
    U16(t + 0xD8) = 0x1F3;
    cam_look(c);
    U16(t + 8) = 0xF830; U16(t + 0xA) = 0xF830; U16(t + 0xC) = 0xF830;    /* the light, the colour matrix */
    U16(t + 0x10) = 0; U16(t + 0x12) = 0; U16(t + 0x14) = 0;
    U16(t + 0x18) = 0; U16(t + 0x1A) = 0; U16(t + 0x1C) = 0;
    U16(t + 0x20) = 0x1770; U16(t + 0x26) = 0x1770; U16(t + 0x2C) = 0x1388;
    U16(t + 0x22) = 0; U16(t + 0x28) = 0; U16(t + 0x2E) = 0;
    U16(t + 0x24) = 0; U16(t + 0x2A) = 0; U16(t + 0x30) = 0;
    U8(t + 0xD2) = 0x50; U8(t + 0xD1) = 0x50; U8(t + 0xD0) = 0x50;
    SetBackColor(0x50, 0x50, 0x50);
    U32(t + 0xD4) = 0x808080;
    U8(t + 6) = (Rand8() & 0xF) > 0xA;
    U32(G() + 8) = 0x6C1500;
    Flag_Set(3, 0, 1);
    End3_Run(t);
}

void DC2CALL End3_Run(gptr t)                                            /* 0x485D50 */
{
    gptr B = Tmp_Push(0x58), a = B, M1 = B + 0x18, L = B + 0x38;
    U16(a + 4) = 0; U16(a + 2) = 0; U16(a) = 0;
    RotMatrix(a, M1);
    SetGeomScreen(U16(t + 0xD8));
    SetGeomOffset(0xA0, 0x78);
    gptr c = t + 0x40;
    cam_view(c, M1, 0xDAC, 0, 0);
    lights(M1, t + 8, L);
    SetBackColor(U8(t + 0xD0), U8(t + 0xD1), U8(t + 0xD2));
    SetLightMatrix(L);
    SetColorMatrix(t + 0x20);
    SetRotMatrix(M1);
    SetTransMatrix(M1);
    End3_Anim(t);
    gte_ldrgb(t + 0xD4);
    End3_Verts(0x600000);
    End3_Shade(0x600000);
    End3_Logo(t);
    u32 k = U32(0x87DE0C) << 12;
    Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
    Tmp_Pop(0x58);
}

void DC2CALL End3Task(void)                                              /* 0x485B70 */
{
    memset(MEM(0x7551E8), 0, 0x37 * 4);
    for (;;) {
        switch (U8(0x7551E8)) {
        case 0: End3_Init(0x7551E8); break;
        case 1: End3_Run(0x7551E8); break;
        default: Orig_Call1(U32(0x738B18 + U8(0x7551E8) * 4), 0x7551E8); break;
        }
        Task_Sleep(1);
    }
}
