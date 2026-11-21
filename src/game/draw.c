/* The frame's drawing (0x4A7250 and its tree): the view matrices of objects,
 * effects and parts, the models (lit vertices, then their triangles and quads),
 * the shadows, the effects' meshes, the parts' sprites, the text layer, the
 * dim bars, the background masks and the rope. */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"
#include "dc2/pcio.h"

#define CAM()       (G() + 0x778)
#define SCR_SZ      0x759F70u   /* screen z of the model's vertices (u32 each) */
#define SCR_RGB     0x75A61Cu   /* their lit colours */
#define SCR_Z       0x75ACACu   /* their z for the order table (u16) */
#define SCR_XY      0x75B028u   /* their screen xy (8 bytes each) */
#define TMP_MAT     0x75AFF8u
#define VIEW_D      0x75B018u   /* a position minus the camera's */
#define POLY_Z      0x75BD48u   /* the 4 screen z of a polygon, then its order table z */
#define CAM_T       0x75A600u   /* the camera's translation (3 x s32) */
#define CAM_P       0x75A610u   /* the camera's position (3 x s16) */
#define FIXED_Z     0x75A618u

static gptr ot_at(u32 z) { return 0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + z); }
static void ot_link(gptr ot, u32 len)
{
    gptr p = U32(0x87DC98);
    U32(p) = U32(ot) | len << 24; U32(ot) = p & 0xFFFFFF;
}
static u32 z_shift(void) { return U8(G() + 0xB4B) & 31; }

/* the x87 compares of the culling: "< 0" (or unordered) and "<= limit" (or unordered) */
static int lt0(float f) { return !(f >= 0.0f); }
static int le(float f, float lim) { return !(f > lim); }
static int zero_or_nan(float f) { return !(f < 0.0f || f > 0.0f); }

/* off screen: all points left, right, above or below */
static int culled(gptr p, const u32 *xo, int n)
{
    int i;
    for (i = 0; i < n && lt0(F32(p + xo[i])); i++) ;
    if (i == n) return 1;
    for (i = 0; i < n && !le(F32(p + xo[i]), 320.0f); i++) ;
    if (i == n) return 1;
    for (i = 0; i < n && lt0(F32(p + xo[i] + 4)); i++) ;
    if (i == n) return 1;
    for (i = 0; i < n && !le(F32(p + xo[i] + 4), 240.0f); i++) ;
    return i == n;
}

/* ---- view matrices ------------------------------------------------------------ */

/* an object's (or effect's) view matrix from its own m */
void DC2CALL Obj_ToView(gptr o, gptr m)                               /* 0x4A71B0 */
{
    MulMatrix0(CAM(), m, o);
    U32(VIEW_D) = (u32)(S16(o + 0x40) - S16(CAM_P));
    U32(VIEW_D + 4) = (u32)(S16(o + 0x42) - S16(CAM_P + 2));
    U32(VIEW_D + 8) = (u32)(S16(o + 0x44) - S16(CAM_P + 4));
    ApplyRotMatrixLV(VIEW_D, o + 0x14);
    U32(o + 0x14) += U32(CAM_T); U32(o + 0x18) += U32(CAM_T + 4); U32(o + 0x1C) += U32(CAM_T + 8);
}

/* the matrices of the objects, their parts, the effects and the props */
void DC2CALL World_Matrices(void)                                     /* 0x4A6DF0 */
{
    gptr c = CAM();
    SetRotMatrix(c);
    U32(CAM_P) = U32(c + 0x28); U16(CAM_P + 4) = U16(c + 0x2C);
    U32(CAM_T) = (u32)(s32)S16(c + 0x30); U32(CAM_T + 4) = (u32)(s32)S16(c + 0x32); U32(CAM_T + 8) = U16(c + 0x34);
    for (gptr o = U32(G() + 0x940); o < U32(G() + 0x940) + U32(0x87F940) * 0x260; o += 0x260) {
        u32 f = U32(o + 0x50);
        if (!(f & 1) || !(f & 2)) continue;
        gptr q = U32(o + 0x64);
        Obj_ToView(o, o + 0x20);
        for (; q; q = U32(q + 0x54)) Obj_ToView(q, q + 0x20);
    }
    for (gptr e = U32(G() + 0x93C); e < U32(G() + 0x93C) + U32(0x87DE04) * 0x70; e += 0x70) {
        u32 f = U32(e + 0x50);
        if (!(f & 1) || !(f & 2)) continue;
        gptr par = U32(e + 0x54);
        if (par) {
            if (!(U8(par + 0x50) & 2)) continue;
            SetRotMatrix(par);
            gte_ldv0(e + 0x40); gte_rtv0(); gte_stlvnl(e + 0x14);
            MulMatrix0(par, e + 0x20, e);
            U32(e + 0x14) += U32(par + 0x14); U32(e + 0x18) += U32(par + 0x18); U32(e + 0x1C) += U32(par + 0x1C);
        } else {
            SetRotMatrix(c);
            Obj_ToView(e, e + 0x20);
        }
    }
    for (gptr p = U32(G() + 0x944); p < U32(G() + 0x944) + U32(0x87DC88) * 0xAC; p += 0xAC) {
        u32 f = U32(p + 0x50);
        if (!(f & 1) || !(f & 2) || (U8(p + 0x84) & 0x80)) continue;
        RotMatrix(p + 0x48, p);
        u8 fl = U8(p + 0x84);
        if (U32(p + 0x60)) {
            SetRotMatrix(p + 0x20);
            gte_ldv0(p + 0x40); gte_rtv0(); gte_stlvnl(p + 0x14);
            U32(p + 0x14) += U32(p + 0x34); U32(p + 0x18) += U32(p + 0x38); U32(p + 0x1C) += U32(p + 0x3C);
            U32(VIEW_D) = U32(p + 0x14) - (u32)(s32)S16(CAM_P);
            U32(VIEW_D + 4) = U32(p + 0x18) - (u32)(s32)S16(CAM_P + 2);
            U32(VIEW_D + 8) = U32(p + 0x1C) - (u32)(s32)S16(CAM_P + 4);
        } else {
            U32(VIEW_D) = (u32)(S16(p + 0x40) - S16(CAM_P));
            U32(VIEW_D + 4) = (u32)(S16(p + 0x42) - S16(CAM_P + 2));
            U32(VIEW_D + 8) = (u32)(S16(p + 0x44) - S16(CAM_P + 4));
        }
        ApplyMatrixLV(c, VIEW_D, p + 0x14);
        U32(p + 0x14) += U32(CAM_T); U32(p + 0x18) += U32(CAM_T + 4); U32(p + 0x1C) += U32(CAM_T + 8);
        if (fl & 1) {
            if (U32(p + 0x60)) {
                SetRotMatrix(p + 0x20);
                for (int k = 0; k < 3; k++) { gte_ldclmv(p + 2 * k); gte_rtir(); gte_stclmv(p + 2 * k); }
            }
            SetRotMatrix(c);
            for (int k = 0; k < 3; k++) { gte_ldclmv(p + 2 * k); gte_rtir(); gte_stclmv(p + 2 * k); }
        }
    }
}

/* the view matrix dst of the world matrix src */
void DC2CALL Mat_ToView(gptr src, gptr dst)                           /* 0x4460A0 */
{
    gptr c = CAM(), t = Tmp_Push(0xC);
    SetRotMatrix(c);
    gte_ldclmv(src); gte_rtir();
    U32(t) = U32(src + 0x14) - (u32)(s32)S16(c + 0x28);
    gte_stclmv(dst);
    gte_ldclmv(src + 2); gte_rtir();
    U32(t + 4) = U32(src + 0x18) - (u32)(s32)S16(c + 0x2A);
    gte_stclmv(dst + 2);
    gte_ldclmv(src + 4); gte_rtir();
    U32(t + 8) = U32(src + 0x1C) - (u32)(s32)S16(c + 0x2C);
    gte_stclmv(dst + 4);
    ApplyMatrixLV(c, t, dst + 0x14);
    U32(dst + 0x14) += (u32)(s32)S16(c + 0x30); U32(dst + 0x18) += (u32)(s32)S16(c + 0x32); U32(dst + 0x1C) += U16(c + 0x34);
    Tmp_Pop(0xC);
}

/* ---- polygon z (the callbacks of the mesh drawers) ----------------------------- */

static void z_store(s32 z)
{
    z >>= z_shift();
    U32(POLY_Z) = (u32)(z > 0x3FF ? 0x3FF : z);
}
void DC2CALL Z_Fixed(void)                                            /* 0x4A82F0 */
{
    s32 z = S32(FIXED_Z);
    if (z > 0x3FF) U32(FIXED_Z) = (u32)(z = 0x3FF);
    U32(POLY_Z) = (u32)z;
}
static s32 zmin(int n) { s32 z = S32(POLY_Z); for (int i = 1; i < n; i++) if (z > S32(POLY_Z + 4 * i)) z = S32(POLY_Z + 4 * i); return z; }
static s32 zmax(int n) { s32 z = S32(POLY_Z); for (int i = 1; i < n; i++) if (z < S32(POLY_Z + 4 * i)) z = S32(POLY_Z + 4 * i); return z; }
void DC2CALL Z_Min3(void) { z_store(zmin(3)); }                       /* 0x4A8310 */
void DC2CALL Z_Min4(void) { z_store(zmin(4)); }                       /* 0x4A8360 */
void DC2CALL Z_Max3(void) { z_store(zmax(3)); }                       /* 0x4A83C0 */
void DC2CALL Z_Max4(void) { z_store(zmax(4)); }                       /* 0x4A8410 */

static void z_call(gptr fn)
{
    switch (fn) {
    case 0x4A82F0: Z_Fixed(); break;
    case 0x4A8310: Z_Min3(); break;
    case 0x4A8360: Z_Min4(); break;
    case 0x4A83C0: Z_Max3(); break;
    case 0x4A8410: Z_Max4(); break;
    default: Orig_Call1(fn, 0); break;
    }
}

/* ---- the effects' meshes (each vertex transformed) ----------------------------- */

/* textured triangles of 0x28 bytes (3 vertices, then the colours) */
void DC2CALL Mesh_TriTex(gptr e, u32 n, gptr zfn)                     /* 0x4A8470 */
{
    static const u32 xo[3] = {8, 0x1C, 0x30};
    gptr t = Tmp_Push(4);
    for (; n; n--, e += 0x28) {
        gte_ldv3c(e); gte_rtpt();
        gptr p = U32(0x87DC98);
        gte_stsxy3_gt3(p); PC_stsz3_gt3(p);
        gte_stflg(t);
        if (U32(t) & 0x80000000u) continue;
        gte_nclip(); gte_stopz(t);
        if (S32(t) < 0) continue;
        if (culled(p, xo, 3)) continue;
        U32(p + 4) = U32(e + 0x1C); U32(p + 0x18) = U32(e + 0x20); U32(p + 0x2C) = U32(e + 0x24);
        gte_stsz3(POLY_Z);
        U8(p + 7) |= 0x34;
        U16(p + 0x2A) = U16(e + 0x18) | 0x2000; U16(p + 0x16) = U16(e + 0x1A);
        U16(p + 0x14) = U16(e + 6); U16(p + 0x28) = U16(e + 0xE); U16(p + 0x3C) = U16(e + 0x16);
        z_call(zfn);
        ot_link(ot_at(U32(POLY_Z)), 9);
        U32(0x87DC98) += 0x40;
    }
    Tmp_Pop(4);
}

/* textured quads of 0x34 bytes */
void DC2CALL Mesh_QuadTex(gptr e, u32 n, gptr zfn)                   /* 0x4A8690 */
{
    static const u32 xo[4] = {8, 0x1C, 0x30, 0x44};
    gptr t = Tmp_Push(4);
    for (; n; n--, e += 0x34) {
        gte_ldv3c(e); gte_rtpt();
        gte_stflg(t);
        if (U32(t) & 0x80000000u) continue;
        gte_nclip(); gte_stopz(t);
        if (S32(t) < 0) continue;
        gptr p = U32(0x87DC98);
        gte_stsxy3_gt3(p);
        gte_ldv0(e + 0x18); gte_rtps();
        gte_stsxy(U32(0x87DC98) + 0x44);
        PC_stsz4_gt4(U32(0x87DC98));
        if (culled(p, xo, 4)) continue;
        U32(p + 4) = U32(e + 0x24); U32(p + 0x18) = U32(e + 0x28); U32(p + 0x2C) = U32(e + 0x2C); U32(p + 0x40) = U32(e + 0x30);
        gte_stsz4(POLY_Z);
        U8(p + 7) |= 0x3C;
        U16(p + 0x2A) = U16(e + 0x20) | 0x2000; U16(p + 0x16) = U16(e + 0x22);
        U16(p + 0x14) = U16(e + 6); U16(p + 0x28) = U16(e + 0xE); U16(p + 0x3C) = U16(e + 0x16); U16(p + 0x50) = U16(e + 0x1E);
        z_call(zfn);
        ot_link(ot_at(U32(POLY_Z)), 0xC);
        U32(0x87DC98) += 0x54;
    }
    Tmp_Pop(4);
}

/* flat textured quads of 0x34 bytes (grey) */
void DC2CALL Mesh_QuadFlat(gptr e, u32 n, gptr zfn)                  /* 0x4A8930 */
{
    static const u32 xo[4] = {8, 0x18, 0x28, 0x38};
    gptr t = Tmp_Push(4);
    for (; n; n--, e += 0x34) {
        gte_ldv3c(e); gte_rtpt();
        gte_stflg(t);
        if (U32(t) & 0x80000000u) continue;
        gte_nclip(); gte_stopz(t);
        if (S32(t) < 0) continue;
        gptr p = U32(0x87DC98);
        gte_stsxy3_g3(p);
        gte_ldv0(e + 0x18); gte_rtps();
        gte_stsxy(U32(0x87DC98) + 0x38);
        PC_stsz4_g4(U32(0x87DC98));
        if (culled(p, xo, 4)) continue;
        gte_stsz4(POLY_Z);
        U16(p + 0x26) = U16(e + 0x20) | 0x2000; U16(p + 0x16) = U16(e + 0x22);
        U32(p + 4) = 0x2C808080;
        U16(p + 0x14) = U16(e + 6); U16(p + 0x24) = U16(e + 0xE); U16(p + 0x34) = U16(e + 0x16); U16(p + 0x44) = U16(e + 0x1E);
        z_call(zfn);
        ot_link(ot_at(U32(POLY_Z)), 9);
        U32(0x87DC98) += 0x48;
    }
    Tmp_Pop(4);
}

/* the effects' meshes, by kind (G+0x93C) */
void DC2CALL Eff_DrawAll(void)                                        /* 0x4A7840 */
{
    u32 sh = U8(G() + 0xB4B);
    for (gptr e = U32(G() + 0x93C); e < U32(G() + 0x93C) + U32(0x87DE04) * 0x70; e += 0x70) {
        u32 f = U32(e + 0x50);
        if (!(f & 1) || !(f & 2)) continue;
        gptr par = U32(e + 0x54);
        if (par && !(U8(par + 0x50) & 2)) continue;
        SetRotMatrix(e); SetTransMatrix(e);
        gptr m = U32(e + 0x68);
        switch (U16(e + 0x6C)) {
        case 0:
            Mesh_QuadTex(U32(m + 4), U16(m + 0xA), 0x4A8360);
            Mesh_TriTex(U32(m), U16(m + 8), 0x4A8310);
            break;
        case 1:
            Mesh_QuadTex(U32(m + 4), U16(m + 0xA), 0x4A8410);
            Mesh_TriTex(U32(m), U16(m + 8), 0x4A83C0);
            break;
        case 4:
            Mesh_QuadFlat(U32(m + 4), U16(m + 0xA), 0x4A8410);
            Mesh_TriTex(U32(m), U16(m + 8), 0x4A83C0);
            break;
        case 2:
            U32(FIXED_Z) = U16(e + 0x6E);
            Mesh_QuadTex(U32(m + 4), U16(m + 0xA), 0x4A82F0);
            Mesh_TriTex(U32(m), U16(m + 8), 0x4A82F0);
            break;
        case 3: {                                   /* at the effect's own depth */
            gptr v = Tmp_Push(8);
            U32(v) = 0; U32(v + 4) = 0;
            gte_ldv0(v); gte_rtps(); gte_stsz(POLY_Z);
            Tmp_Pop(8);
            s32 z = S32(POLY_Z) >> (sh & 31);
            U32(FIXED_Z) = (u32)(z >= 0x3FF ? 0x3FF : z);
            Mesh_QuadTex(U32(m + 4), U16(m + 0xA), 0x4A82F0);
            Mesh_TriTex(U32(m), U16(m + 8), 0x4A82F0);
            break; }
        }
    }
}

/* ---- models (vertices lit and projected once, then indexed polygons) ------------ */

/* textured triangles of 0xC bytes (3 vertex indices, uv) */
void DC2CALL Model_Tris(gptr e, u32 n, u32 tp)                        /* 0x4A8C00 */
{
    static const u32 xo[3] = {8, 0x1C, 0x30};
    gptr t = Tmp_Push(4);
    for (; n; n--, e += 0xC) {
        u32 i[3] = {U16(e), U16(e + 2), U16(e + 4)};
        gte_ldsxy3(SCR_XY + 8 * i[0], SCR_XY + 8 * i[1], SCR_XY + 8 * i[2]);
        gte_nclip(); gte_stopz(t);
        if (S32(t) < 0) continue;
        gptr p = U32(0x87DC98);
        gte_stsxy3_gt3(p);
        if (culled(p, xo, 3)) continue;
        for (int k = 0; k < 3; k++) {
            s32 sc = (s32)(0x400u << ((z_shift() + 1) & 31));
            F32(p + 0x10 + 0x14 * k) = (float)((xreal)U32(SCR_SZ + 4 * i[k]) / (xreal)sc);
        }
        if (zero_or_nan(F32(p + 0x10)) || zero_or_nan(F32(p + 0x24)) || zero_or_nan(F32(p + 0x38))) continue;
        U32(p + 4) = U32(SCR_RGB + 4 * i[0]); U32(p + 0x18) = U32(SCR_RGB + 4 * i[1]); U32(p + 0x2C) = U32(SCR_RGB + 4 * i[2]);
        U16(p + 0x14) = U16(e + 8); U16(p + 0x28) = U16(e + 0xA); U16(p + 0x3C) = U16(e + 6);
        U8(p + 7) |= 0x34;
        U16(p + 0x2A) = (u16)tp | 0x2000; U16(p + 0x16) = (u16)(tp >> 16);
        u16 z = U16(SCR_Z + 2 * i[0]);
        if (z < U16(SCR_Z + 2 * i[1])) z = U16(SCR_Z + 2 * i[1]);
        if (z < U16(SCR_Z + 2 * i[2])) z = U16(SCR_Z + 2 * i[2]);
        ot_link(ot_at(z), 9);
        U32(0x87DC98) += 0x40;
    }
    Tmp_Pop(4);
}

/* textured quads of 0x10 bytes (4 vertex indices, uv) */
void DC2CALL Model_Quads(gptr e, u32 n, u32 tp)                       /* 0x4A8F40 */
{
    static const u32 xo[4] = {8, 0x1C, 0x30, 0x44};
    gptr t = Tmp_Push(4);
    for (; n; n--, e += 0x10) {
        u32 i[4] = {U16(e), U16(e + 2), U16(e + 4), U16(e + 6)};
        gte_ldsxy3(SCR_XY + 8 * i[0], SCR_XY + 8 * i[1], SCR_XY + 8 * i[2]);
        gte_nclip(); gte_stopz(t);
        if (S32(t) < 0) continue;
        gptr p = U32(0x87DC98);
        gte_stsxy3_gt3(p);
        U32(p + 0x44) = U32(SCR_XY + 8 * i[3]); U32(p + 0x48) = U32(SCR_XY + 8 * i[3] + 4);
        if (culled(p, xo, 4)) continue;
        for (int k = 0; k < 4; k++) {
            s32 sc = (s32)(0x400u << ((z_shift() + 1) & 31));
            F32(p + 0x10 + 0x14 * k) = (float)((xreal)U32(SCR_SZ + 4 * i[k]) / (xreal)sc);
        }
        if (zero_or_nan(F32(p + 0x10)) || zero_or_nan(F32(p + 0x24)) || zero_or_nan(F32(p + 0x38)) || zero_or_nan(F32(p + 0x4C))) continue;
        for (int k = 0; k < 4; k++) U32(p + 4 + 0x14 * k) = U32(SCR_RGB + 4 * i[k]);
        U16(p + 0x14) = U16(e + 8); U16(p + 0x28) = U16(e + 0xA); U16(p + 0x3C) = U16(e + 0xC); U16(p + 0x50) = U16(e + 0xE);
        U8(p + 7) |= 0x3C;
        U16(p + 0x2A) = (u16)tp | 0x2000; U16(p + 0x16) = (u16)(tp >> 16);
        u16 z = U16(SCR_Z + 2 * i[0]);
        for (int k = 1; k < 4; k++) if (z < U16(SCR_Z + 2 * i[k])) z = U16(SCR_Z + 2 * i[k]);
        ot_link(ot_at(z), 0xC);
        U32(0x87DC98) += 0x54;
    }
    Tmp_Pop(4);
}

/* a model's groups (0x14 bytes from +0x24, count +0x14) whose bit is set in mask */
void DC2CALL Model_DrawGroups(gptr m, u32 mask, u32 tp)               /* 0x4A8BA0 */
{
    s32 n = S32(m + 0x14);
    if (n <= 0) return;
    for (gptr g = m + 0x24; n; n--, g += 0x14, mask >>= 1)
        if (mask & 1) {
            Model_Tris(U32(g), U16(g + 8), tp);
            Model_Quads(U32(g + 4), U16(g + 0xA), tp);
        }
}

/* the light matrix of a part (the light set's directions turned into it) */
static void part_light(gptr q)
{
    TransposeMatrix(q + 0x20, TMP_MAT);
    SetRotMatrix(TMP_MAT);
    gte_ldv3c(q + 0x74);
    gte_rtv0(); gte_stsv(TMP_MAT);
    gte_rtv1(); gte_stsv(TMP_MAT + 6);
    gte_rtv2(); gte_stsv(TMP_MAT + 0xC);
    SetLightMatrix(TMP_MAT);
    SetColorMatrix(q + 0xAC);
}

/* the objects' models: each part's vertices projected and lit, then the groups */
void DC2CALL Obj_DrawAll(void)                                        /* 0x4A7510 */
{
    u32 sh = U8(G() + 0xB4B);
    for (gptr o = U32(0x87E760); o < U32(0x87E760) + U32(0x87F940) * 0x260; o += 0x260) {
        u32 f = U32(o + 0x50);
        if (!(f & 1) || !(f & 2)) continue;
        gptr q = U32(o + 0x64), m = U32(o + 0x60);
        u32 mask = 0xFFFFFFFF, idx = 0;
        gptr v = U32(m), nv = U32(m + 4), xy = SCR_XY, rgb = SCR_RGB, sz = SCR_SZ, zz = SCR_Z;
        part_light(q);
        for (; q; q = U32(q + 0x54)) {
            gte_ldrgb(q + 0x8C);
            if (U32(q + 0x50) & 0x10000000) mask &= ~(1u << (idx & 31));
            idx++;
            if (U8(o + 0xFE) & 1) part_light(q);
            u32 n = U8(q + 0x62);
            SetRotMatrix(q); SetTransMatrix(q);
            for (; n; n--) {
                gte_ldv3c(v); gte_rtpt();
                gte_stsxy3c(xy); gte_stsz3(sz);
                gte_ldv3c(nv); gte_ncct();
                for (int k = 0; k < 3; k++)
                    if ((U32(sz + 4 * k) >> (sh & 31)) > 0x3FF) U32(sz + 4 * k) = 0x3FFu << (sh & 31);
                for (int k = 0; k < 3; k++) U16(zz + 2 * k) = (u16)(U32(sz + 4 * k) >> (sh & 31));
                zz += 6; sz += 0xC;
                v += 0x18; nv += 0x18; xy += 0x18;
                gte_strgb3(rgb); rgb += 0xC;
            }
        }
        Model_DrawGroups(U32(o + 0x60), mask, (u32)U16(o + 0x106) << 16 | U16(o + 0x104));
    }
}

/* ---- shadows ---------------------------------------------------------------------- */

/* a shadow quad (0x190 a side, scaled by +0x110/+0x112) under the objects marked
   by +0x118 bit 7, on the floor by +0x118 bits 4-5 */
void DC2CALL Obj_DrawShadows(void)                                    /* 0x4A7ED0 */
{
    u32 sh = U8(G() + 0xB4B);
    gptr L = Tmp_Push(0x38), r = L, sc = L + 8, q = L + 0x18;   /* q: the four corners */
    static const s16 cn[4][2] = {{-0x190, 0x190}, {0x190, 0x190}, {-0x190, -0x190}, {0x190, -0x190}};
    for (int k = 0; k < 4; k++) { U16(q + 8 * k) = (u16)cn[k][0]; U16(q + 8 * k + 2) = 0; U16(q + 8 * k + 4) = (u16)cn[k][1]; U16(q + 8 * k + 6) = 0; }
    for (gptr o = U32(G() + 0x940); o < U32(G() + 0x940) + U32(0x87F940) * 0x260; o += 0x260) {
        u32 f = U32(o + 0x50);
        if (!(f & 1) || !(f & 2) || !(U8(o + 0x118) & 0x80)) continue;
        U16(r + 4) = 0; U16(r) = 0;
        U16(r + 2) = (u16)(U16(U32(o + 0x64) + 0x4A) + U16(o + 0x4A));
        RotMatrix(r, TMP_MAT);
        U32(sc) = U16(o + 0x110); U32(sc + 4) = 0; U32(sc + 8) = U16(o + 0x112);
        ScaleMatrix(TMP_MAT, sc);
        gptr pt = Obj_Part(U32(o + 0x64), U8(o + 0x118) & 0xF);
        SetRotMatrix(CAM());
        gte_ldclmv(TMP_MAT); gte_rtir();
        U16(r) = (u16)(U16(pt + 0x40) - U16(G() + 0x7A0));
        gte_stclmv(o);
        gte_ldclmv(TMP_MAT + 2); gte_rtir();
        U16(r + 4) = (u16)(U16(pt + 0x44) - U16(G() + 0x7A4));
        gte_stclmv(o + 2);
        gte_ldclmv(TMP_MAT + 4); gte_rtir();
        switch (U8(o + 0x118) & 0x30) {
        case 0x20: {
            u32 m = o != U32(G() + 0x940) ? 0x40 : 0x80;
            if (U32(o + 0x50) & 0x1000) m = 0x20;
            u16 y = (u16)Floor_Height(pt + 0x40, pt + 0x40, m, 0);
            U16(r + 2) = y; U16(o + 0x10E) = y;
            U16(r + 2) -= U16(G() + 0x7A2);
            break; }
        case 0x10: U16(r + 2) = (u16)(U16(o + 0x42) - U16(G() + 0x7A2)); break;
        case 0x00: U16(r + 2) = (u16)((u16)(U16(o + 0x5A) * 0xFC18) - U16(G() + 0x7A2)); break;
        }
        gte_stclmv(o + 4);
        gte_ldv0(r); gte_rtv0(); gte_stlvnl(o + 0x14);
        U32(o + 0x14) += (u32)(s32)S16(G() + 0x7A8);
        U32(o + 0x18) += (u32)(s32)S16(G() + 0x7AA);
        U32(o + 0x1C) += U16(G() + 0x7AC);
        SetRotMatrix(o); SetTransMatrix(o);
        gte_ldv3c(q); gte_rtpt();
        gptr p = U32(0x87DC98);
        U8(p + 0x34) = 0; U8(p + 0x14) = 0; U8(p + 0x25) = 0x48; U8(p + 0x15) = 0x48;
        gte_stsxy3_g3(p);
        gte_ldv0(q + 0x18); gte_rtps();
        p = U32(0x87DC98);
        U8(p + 0x44) = 0x2F; U8(p + 0x24) = 0x2F; U8(p + 0x45) = 0x77; U8(p + 0x35) = 0x77;
        gte_stsxy(p + 0x38);
        PC_stsz4_g4(p);
        gte_stsz4(POLY_Z);
        s32 z = zmax(4) >> (sh & 31);
        U32(POLY_Z) = (u32)(z >= 0x3FF ? 0x3FF : z);
        U16(p + 0x16) = 0x7DC1; U16(p + 0x26) = 0x84B; U32(p + 4) = 0x2E202020; U16(p + 0x26) = 0x44B;
        ot_link(ot_at(U32(POLY_Z)), 9);
        U32(0x87DC98) += 0x48;
    }
    Tmp_Pop(0x38);
}

/* ---- props' sprites and quads ------------------------------------------------------ */

/* the props (G+0x944): sprite lists (+0x84 bit 7) or textured quads by the list
   at +0x94 (u, v, w, h, count, -, dx, dy) */
void DC2CALL Prop_DrawAll(void)                                       /* 0x4A7A20 */
{
    s32 sh2 = (s32)U8(G() + 0xB4B) - 2;
    gptr V = Tmp_Push(0x18);
    for (gptr p = U32(G() + 0x944); p < U32(G() + 0x944) + U32(0x87DC88) * 0xAC; p += 0xAC) {
        u32 f = U32(p + 0x50);
        if (!(f & 1) || !(f & 2)) continue;
        gptr par = U32(p + 0x60);
        if (par && !(U8(par + 0x50) & 2)) continue;
        if (U8(p + 0x84) & 0x80) {
            u32 col = U32(p + 0x64);
            u32 code = (col & 0xFFFFFF) | (col & 0x2000000 ? 0x66000000 : 0x64000000);
            gptr e = U32(p + 0x94);
            for (u32 n = U8(e + 4) & 0x1F; n; n--, e += 8) {
                gptr q = U32(0x87DC98);
                F32(q + 8) = (float)(S8(e + 6) + S16(p + 0x40));
                F32(q + 0xC) = (float)(S8(e + 7) + S16(p + 0x42));
                U32(q + 0x10) = 0x3C23D70A;
                U16(q + 0x18) = U8(e + 2); U16(q + 0x1A) = U8(e + 3);
                U16(q + 0x16) = U16(p + 0x68);
                U8(q + 0x14) = U8(e); U8(q + 0x15) = U8(e + 1);
                U32(q + 4) = code;
                ot_link(ot_at((u32)S16(p + 0x44)), 4);
                U32(0x87DC98) += 0x1C;
            }
            Gpu_SetDrawMode(U32(0x87DC98), 1, 0, U16(p + 0x6A), 0);
            Gpu_AddPrim(ot_at((u32)S16(p + 0x44)), U32(0x87DC98));
            U32(0x87DC98) += 0xC;
            continue;
        }
        SetRotMatrix(p); SetTransMatrix(p);
        gptr s = U32(p + 0x94);
        u16 sx = U16(p + 0x6C), sy = U16(p + 0x6E);
        for (u32 n = U8(s + 4) & 0x1F; n; n--, s += 8) {
            u16 x0 = (u16)S8(s + 6), y0 = (u16)S8(s + 7), x1 = (u16)(x0 + U8(s + 2)), y1 = (u16)(y0 + U8(s + 3));
            U16(V) = (u16)(x0 * sx); U16(V + 2) = (u16)(y0 * sy); U16(V + 4) = 0;
            U16(V + 8) = (u16)(x1 * sx); U16(V + 0xA) = (u16)(y0 * sy); U16(V + 0xC) = 0;
            U16(V + 0x10) = (u16)(x0 * sx); U16(V + 0x12) = (u16)(y1 * sy); U16(V + 0x14) = 0;
            gte_ldv3c(V); gte_rtpt();
            gptr q = U32(0x87DC98);
            U8(q + 0x34) = U8(s); U8(q + 0x14) = U8(s); U8(q + 0x25) = U8(s + 1); U8(q + 0x15) = U8(s + 1);
            gte_stsxy3_g3(q);
            U16(V) = (u16)(x1 * sx); U16(V + 2) = (u16)(y1 * sy); U16(V + 4) = 0;
            gte_ldv0(V); gte_rtps();
            U8(q + 0x44) = U8(q + 0x24) = (u8)(U8(s) + U8(s + 2) - 1);
            U8(q + 0x45) = U8(q + 0x35) = (u8)(U8(s + 3) + U8(s + 1) - 1);
            gte_stsxy(q + 0x38);
            PC_stsz4_g4(q);
            U16(q + 0x16) = U16(p + 0x68); U32(q + 4) = U32(p + 0x64);
            gte_stsz4(POLY_Z);
            s32 z;
            if (U8(p + 0x84) & 4) z = zmax(4);
            else z = (S32(POLY_Z + 0xC) + S32(POLY_Z + 8) + S32(POLY_Z + 4) + S32(POLY_Z)) >> 4;
            z >>= (sh2 & 31);
            U32(POLY_Z) = (u32)(z >= 0x3FF ? 0x3FF : z);
            U16(q + 0x26) = U16(p + 0x6A) | 0x400;
            if ((U32(p + 0x64) & 0xFFFFFF) == 0xD00000) U8(q + 0x27) |= 0x10;
            ot_link(ot_at(U32(POLY_Z)), 9);
            U32(0x87DC98) += 0x48;
        }
    }
    Tmp_Pop(0x18);
}

/* ---- the rest of the frame ----------------------------------------------------------- */

/* the handlers of the 0x34-byte entries at G+0x948 */
void DC2CALL Draw_Handlers(void)                                      /* 0x4A7330 */
{
    for (gptr e = U32(G() + 0x948); e < U32(G() + 0x948) + U32(0x87DE14) * 0x34; e += 0x34)
        if ((U8(e) & 1) && U32(e + 4)) Orig_Call1(U32(e + 4), e);
}

/* the two dim bars (top and bottom, 0x1C high) by G+0xB10 */
void DC2CALL Dim_Bars(void)                                           /* 0x4A7390 */
{
    s32 v = S16(G() + 0xB10);
    if (!v) return;
    for (int k = 0; k < 2; k++) {
        gptr p = U32(0x87DC98);
        Gpu_SetTile(p); Gpu_SetSemiTrans(p, 1);
        U32(p + 8) = 0; U32(p + 0xC) = 0; U16(p + 0x14) = 0x140; U16(p + 0x16) = 0x1C;
        if (k) U32(p + 0xC) = 0x43540000;                    /* 212 */
        U8(p + 6) = (u8)v; U8(p + 5) = (u8)v; U8(p + 4) = (u8)v;
        Gpu_AddPrim(U32(0x87F94C) + 0x78, p);
        U32(0x87DC98) += 0x18;
    }
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, Gpu_GetTPage(0, 2, 0x140, 0x100) & 0xFFFF, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x78, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* the background masks of the camera c (re-depthed when G+0xC27 changed) */
void DC2CALL Masks_Draw(gptr c)                                       /* 0x403760 */
{
    if (!U16(c + 0x50)) return;
    u32 d = U8(G() + 0xC27);
    if (U32(0x746C9C) != d) {
        Masks_ShiftDepth(d - U32(0x746C9C));
        Masks_Upload(0);
        U32(0x746C9C) = U8(G() + 0xC27);
    }
    for (s32 i = 0; i < (s32)(U16(c + 0x50) * 6);) {
        gptr p = U32(0x87DC98);
        SetMaskMarker(p);
        u32 z = Masks_FillMarker((u32)i, p);
        i += U16(p + 0xC);
        ot_link(0x87BC00 + 4 * ((U32(0x87DE0C) << 10) + z), 2);
        U32(0x87DC98) += 0x10;
    }
}

/* the rope (effect 0 with +0x50 bit 15): segments between the matrices at
   0x74AFB0, coloured down from count * step */
void DC2CALL Rope_Draw(void)                                          /* 0x445E10 */
{
    if (!Flag_Check(3, 2)) return;
    gptr e = U32(G() + 0x93C);
    if (!U32(0x87DE04)) return;
    u32 f = U32(e + 0x50);
    if (!(f & 1) || !(f & 2) || !(f & 0x8000) || !(U8(U32(G() + 0x940) + 0x50) & 2)) return;
    gptr tab; u32 step;
    if (!U8(e + 0x4F)) { tab = 0x721428; step = U32(0x721448 + 4 * S16(e + 0x5E)); }
    else { tab = 0x721438; step = U32(0x721450 + 4 * S16(e + 0x5E)); }
    u32 c = (u32)(s32)S16(e + 0x66) * step;
    if (!S16(e + 0x66)) return;
    gptr M = Tmp_Push(0x20), z = Tmp_Push(4);
    u32 col = c | 0x3A000000;
    for (u32 k = 0, m = 0x74AFD0; k < (u32)(s32)S16(e + 0x66); k++, m += 0x20) {
        Mat_ToView(m - 0x20, M);
        SetRotMatrix(M); SetTransMatrix(M);
        gte_ldv0(tab); gte_rtps(); gte_stsxy(U32(0x87DC98) + 8);
        gte_ldv0(tab + 8); gte_rtps(); gte_stsxy(U32(0x87DC98) + 0x18);
        Mat_ToView(m, M);
        SetRotMatrix(M); SetTransMatrix(M);
        gte_ldv0(tab); gte_rtps(); gte_stsxy(U32(0x87DC98) + 0x28);
        gte_ldv0(tab + 8); gte_rtps(); gte_stsxy(U32(0x87DC98) + 0x38);
        PC_stsz4_g4(U32(0x87DC98));
        gte_stsz(z);
        u32 zz = U32(z) >> z_shift();
        if (zz >= 0x3FF) zz = 0x3FF;
        gptr p = U32(0x87DC98);
        U32(p + 0x14) = col;
        U32(p + 4) = U32(p + 0x14);
        c -= step; col = c | 0x3A000000;
        U32(p + 0x34) = col; U32(p + 0x24) = U32(p + 0x34);
        ot_link(ot_at(zz), 8);
        U32(0x87DC98) += 0x44;
        Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0xA5, 0);
        Gpu_AddPrim(ot_at(zz), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
    }
    Tmp_Pop(4); Tmp_Pop(0x20);
}

/* ---- the text layer --------------------------------------------------------------- */

/* the glyph quad template (13 dwords copied to the primitive at +4) */
static void glyph_quad(gptr T, xreal x, float y)
{
    float x0 = (float)x, x1 = (float)(x + XR(14.0)), y1 = (float)((xreal)y + XR(14.0));
    F32(T + 4) = x0; F32(T + 8) = y;
    F32(T + 0x10) = x1; F32(T + 0x14) = y;
    F32(T + 0x1C) = x0; F32(T + 0x20) = y1;
    F32(T + 0x28) = x1; F32(T + 0x2C) = y1;
    gptr p = U32(0x87DC98);
    memmove(MEM(p + 4), MEM(T), 0x34);
    ot_link(0x87F964 + (U32(0x87DE0C) << 4), 2);
    U32(0x87DC98) += 0x38;
}

/* the message slots (0x74D980, 32 of 0x10) and the 0x8C characters of the
   sentence at 0x74D750 with its cursor; then the text order table drawn */
void DC2CALL Text_DrawAll(void)                                       /* 0x471440 */
{
    gptr T = Tmp_Push(0x34);                     /* the template (a stack block of the original) */
    memset(MEM(T), 0, 0x34);
    U8(T + 3) = 0x6C; U16(T + 0xE) = 0x7DCC;
    U8(T + 0x18) = 0xE; U8(T + 0x25) = 0xE; U8(T + 0x30) = 0xE; U8(T + 0x31) = 0xE;
    u16 tp = 0;
    int cursor = 0;
    for (gptr s = 0x74D980; s < 0x74DB80; s += 0x10) {
        if (!(U8(s) & 1)) continue;
        xreal x = (xreal)S16(s + 0xC);
        float y = (float)S16(s + 0xE);
        gptr t = U32(s + 4);
        if (U8(s + 1) == 4) {                           /* a primitive */
            gptr p = U32(0x87DC98);
            U32(p) = U32(t); U32(p + 4) = U32(t + 4); U32(p + 8) = U32(t + 8);
            Gpu_AddPrim(0x87F964 + (U32(0x87DE0C) << 4), p);
            U32(0x87DC98) += 0xC;
            continue;
        }
        for (;; t += 2) {
            u16 c = U16(t);
            u32 k = c >> 12;
            if (k == 0) {
                u32 col = U32(s + 8);
                U8(T) = (u8)col; U8(T + 1) = (u8)(col >> 8); U8(T + 2) = (u8)(col >> 16);
                tp = c & 0xFFF; U16(T + 0x1A) = tp;
                glyph_quad(T, x, y);
                x += (xreal)F32(0x723D4C + 4 * tp);
            } else if (k == 1) U32(s + 8) = U32(0x723F5C + 4 * (c & 0xF));
            else if (k == 4) { x = (xreal)S16(s + 0xC); y = (float)((xreal)y + XR(16.0)); }
            if (k == 0xF) break;
        }
    }
    u32 m = U32(0x87FF90);
    xreal x = (xreal)(s32)((m & 0xFF) * 2);
    float y = (float)(s32)(((m >> 8) & 0xFF) * 2);
    for (u32 i = 0; i < 0x8C; i++) {
        u32 d = U32(0x74D750 + 4 * i);
        if (d >> 24 == 1) {
            gptr c = 0x723F4C + 3 * ((d >> 16) & 0xFF);
            U8(T) = U8(c); U8(T + 1) = U8(c + 1); U8(T + 2) = U8(c + 2);
            tp = U16(0x74D750 + 4 * i) & 0xFFF; U16(T + 0x1A) = tp;
            glyph_quad(T, x, y);
        } else if (d >> 24 == 2 && U16(0x87FF8C) < 0xA && (u8)d == U8(0x87FF8F)) {
            gptr p = U32(0x87DC98);
            U8(p + 4) = 0xFF; U8(p + 5) = 0xFF; U8(p + 6) = 0xFF; U8(p + 7) = 0x64;
            F32(p + 8) = (float)x; F32(p + 0xC) = y; U32(p + 0x10) = 0x3C23D70A;
            U8(p + 0x14) = 0x10; U8(p + 0x15) = 0xF0; U16(p + 0x16) = 0x7DCD; U16(p + 0x18) = 0x10; U16(p + 0x1A) = 0x10;
            ot_link(0x87F96C + (U32(0x87DE0C) << 4), 4);
            U32(0x87DC98) += 0x1C;
            cursor = 1;
        }
        if ((d & 0xFF000000) == 0x2000000) x += XR(12.0);
        else x += (xreal)F32(0x723D4C + 4 * tp);
        if (i == 0x45) { x = (xreal)(s32)((U32(0x87FF90) & 0xFF) * 2); y = (float)((xreal)y + XR(14.0)); }
    }
    Tmp_Pop(0x34);
    if (cursor) {
        Gpu_SetDrawMode(U32(0x87DC98), 1, 1, 0x2B, 0);
        Gpu_AddPrim(0x87F96C + (U32(0x87DE0C) << 4), U32(0x87DC98));
        U32(0x87DC98) += 0xC;
    }
    Gpu_DrawOT(U32(0x87F94C) + 0x74, 0x87F96C + (U32(0x87DE0C) << 4), 0x87F960 + (U32(0x87DE0C) << 4));
}

/* ---- the frame ---------------------------------------------------------------------- */

void DC2CALL Frame_Draw(void)                                         /* 0x4A7250 */
{
    World_Matrices();
    if (Flag_Check(2, 0x12)) DrawSync(0);          /* (called without an argument) */
    Dim_Bars();
    if (Flag_Check(3, 5)) Masks_Draw(G() + 0x778);
    if (Flag_Check(3, 1)) Obj_DrawAll();
    if (Flag_Check(3, 3)) Obj_DrawShadows();
    if (Flag_Check(3, 2)) Eff_DrawAll();
    if (Flag_Check(3, 4)) Prop_DrawAll();
    Text_DrawAll();
    if (Flag_Check(3, 8)) Draw_Handlers();
    Rope_Draw();
    u32 k = U32(0x87DE0C) << 12;
    Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
}
