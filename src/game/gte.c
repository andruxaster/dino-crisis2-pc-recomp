/* GTE emulation of the PC port, see include/dc2/gte.h.
 * MATRIX: s16 m[3][3] (+0), pad (+0x12), s32 t[3] (+0x14).
 * SVECTOR: s16 x, y, z, pad. VECTOR: s32 x, y, z, pad. */
#include "dc2/gte.h"
#include "dc2/orig.h"
#include <math.h>

#define M(m, i, j) S16((m) + 2 * ((i) * 3 + (j)))

/* MSVC _ftol (0x4DD89C): truncate toward zero to 64 bits, low 32 bits returned */
static s32 ftol(long double x) { return (s32)(int64_t)x; }

/* ---- trigonometry, roots ------------------------------------------------- */
s32 DC2CALL rsin(s32 a)                                              /* 0x4C08E0 */
{
    s32 sign = (a & 0x800) ? -1 : 1;
    a &= 0x7FF;
    if (a < 0x400) return S16(GTE_SINTAB + 2 * a) * sign;
    return S16(GTE_SINTAB + 2 * (0x800 - a)) * sign;
}

s32 DC2CALL rcos(s32 a) { return rsin(a + 0x400); }                  /* 0x4C0920 */

s32 DC2CALL ratan2(float y, float x)                                 /* 0x4C0940 */
{
    /* fpatan, * 2048.0, * 0.3184713375796178 (1/3.14) */
    return ftol(atan2l((long double)y, (long double)x) * 2048.0 * 0.3184713375796178);
}

s32 DC2CALL SquareRoot0(s32 x) { return ftol(sqrtl((long double)x)); }               /* 0x4C0960 */
s32 DC2CALL SquareRoot12(s32 x) { return ftol(sqrtl((long double)x * 0.000244140625)); } /* 0x4C0970 */

/* ---- geometry state ------------------------------------------------------ */
void DC2CALL InitGeom(void)                                          /* 0x4C0990 */
{
    S32(GTE_H) = 1000;
    S32(GTE_ZNEAR) = 500;
    S32(0x87B3D0) = 0x3333;          /* depth cue near/far per 1000 of H */
    S32(0x87B41C) = 0x10000;
    S32(GTE_DQNEAR) = 0x3333;
    S32(GTE_DQFAR) = 0x10000;
    S32(0x87B3A0) = 0;
    U32(GTE_ZSCALE) = 0x47000000;    /* 32768.0f */
}

void DC2CALL SetGeomOffset(s32 ofx, s32 ofy)                         /* 0x4C09E0 */
{
    S32(GTE_OFX) = ofx;
    S32(GTE_OFY) = ofy;
}

s32 DC2CALL ReadGeomScreen(void) { return S32(GTE_H); }              /* 0x4C0A00 */

void DC2CALL SetGeomScreen(s32 h)                                    /* 0x4C0A10 */
{
    S32(GTE_H) = h;
    S32(GTE_ZNEAR) = h / 2;
    S32(GTE_DQNEAR) = (s32)((u32)S32(0x87B3D0) * (u32)h) / 1000;
    S32(GTE_DQFAR) = (s32)((u32)S32(0x87B41C) * (u32)h) / 1000;
}

void DC2CALL SetBackColor(s32 r, s32 g, s32 b)                       /* 0x4C0A70 */
{
    U16(GTE_BK) = (u16)(r << 4);
    U16(GTE_BK + 2) = (u16)(g << 4);
    U16(GTE_BK + 4) = (u16)(b << 4);
}

/* ---- matrix * vector ------------------------------------------------------ */
static s32 dot3(gptr m, int row, s32 x, s32 y, s32 z)
{
    return (s32)((u32)(M(m, row, 0) * x) + (u32)(M(m, row, 1) * y) + (u32)(M(m, row, 2) * z));
}

gptr DC2CALL ApplyMatrix(gptr m, gptr sv, gptr v)                    /* 0x4C0AA0 */
{
    s32 x = S16(sv), y = S16(sv + 2), z = S16(sv + 4);
    s32 a = dot3(m, 0, x, y, z) >> 12, b = dot3(m, 1, x, y, z) >> 12, c = dot3(m, 2, x, y, z) >> 12;
    S32(v + 8) = c; S32(v) = a; S32(v + 4) = b;
    return v;
}

gptr DC2CALL ApplyMatrixSV(gptr m, gptr sv, gptr out)                /* 0x4C0B20 */
{
    s32 x = S16(sv), y = S16(sv + 2), z = S16(sv + 4);
    s32 a = dot3(m, 0, x, y, z) >> 12, b = dot3(m, 1, x, y, z) >> 12, c = dot3(m, 2, x, y, z) >> 12;
    S16(out + 4) = (s16)c; S16(out) = (s16)a; S16(out + 2) = (s16)b;
    return out;
}

gptr DC2CALL ApplyMatrixLV(gptr m, gptr v, gptr out)                 /* 0x4C0BA0 */
{
    s32 x = S32(v), y = S32(v + 4), z = S32(v + 8);
    s32 a = dot3(m, 0, x, y, z) >> 12, b = dot3(m, 1, x, y, z) >> 12, c = dot3(m, 2, x, y, z) >> 12;
    S32(out + 8) = c; S32(out) = a; S32(out + 4) = b;
    return out;
}

gptr DC2CALL ApplyRotMatrixLV(gptr v, gptr out)                      /* 0x4C0C20 */
{
    /* each row reads v again after the previous store (matters when out == v) */
    for (int i = 0; i < 3; i++)
        S32(out + 4 * i) = dot3(GTE_RT, i, S32(v), S32(v + 4), S32(v + 8)) >> 12;
    return out;
}

/* ---- matrices ------------------------------------------------------------ */
gptr DC2CALL MulMatrix0(gptr m0, gptr m1, gptr m2)                   /* 0x4C0CB0 */
{
    s16 r[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r[i * 3 + j] = (s16)((s32)((u32)(M(m0, i, 0) * M(m1, 0, j)) + (u32)(M(m0, i, 1) * M(m1, 1, j)) +
                                       (u32)(M(m0, i, 2) * M(m1, 2, j))) >> 12);
    /* the original copies 20 bytes: the pad word at +0x12 gets stack garbage */
    for (int k = 0; k < 9; k++) S16(m2 + 2 * k) = r[k];
    return m2;
}

static gptr rot_axis(gptr m, s16 m00, s16 m01, s16 m02, s16 m10, s16 m11, s16 m12,
                     s16 m20, s16 m21, s16 m22)
{
    gptr t = Tmp_Push(20);
    S16(t + 0) = m00; S16(t + 2) = m01; S16(t + 4) = m02;
    S16(t + 6) = m10; S16(t + 8) = m11; S16(t + 10) = m12;
    S16(t + 12) = m20; S16(t + 14) = m21; S16(t + 16) = m22;
    MulMatrix0(t, m, m);
    Tmp_Pop(20);
    return m;
}

gptr DC2CALL RotMatrixX(s32 r, gptr m)                               /* 0x4C0E50 */
{
    s16 c = (s16)rcos(r), s = (s16)rsin(r);
    return rot_axis(m, 0x1000, 0, 0, 0, c, (s16)-s, 0, s, c);
}

gptr DC2CALL RotMatrixY(s32 r, gptr m)                               /* 0x4C0EC0 */
{
    s16 c = (s16)rcos(r), s = (s16)rsin(r);
    return rot_axis(m, c, 0, s, 0, 0x1000, 0, (s16)-s, 0, c);
}

gptr DC2CALL RotMatrixZ(s32 r, gptr m)                               /* 0x4C0F30 */
{
    s16 c = (s16)rcos(r), s = (s16)rsin(r);
    return rot_axis(m, c, (s16)-s, 0, s, c, 0, 0, 0, 0x1000);
}

static void identity(gptr m) { memcpy(MEM(m), MEM(GTE_IDENT), 20); }

gptr DC2CALL RotMatrix(gptr r, gptr m)                               /* 0x4C0FA0: Rx*Ry*Rz */
{
    identity(m);
    RotMatrixZ(S16(r + 4), m); RotMatrixY(S16(r + 2), m); RotMatrixX(S16(r), m);
    return m;
}

gptr DC2CALL RotMatrixYXZ(gptr r, gptr m)                            /* 0x4C0FF0: Ry*Rx*Rz */
{
    identity(m);
    RotMatrixZ(S16(r + 4), m); RotMatrixX(S16(r), m); RotMatrixY(S16(r + 2), m);
    return m;
}

gptr DC2CALL RotMatrixZXY(gptr r, gptr m)                            /* 0x4C1040: Rz*Rx*Ry */
{
    identity(m);
    RotMatrixY(S16(r + 2), m); RotMatrixX(S16(r), m); RotMatrixZ(S16(r + 4), m);
    return m;
}

gptr DC2CALL RotMatrixZYX(gptr r, gptr m)                            /* 0x4C1090: Rz*Ry*Rx */
{
    identity(m);
    RotMatrixX(S16(r), m); RotMatrixY(S16(r + 2), m); RotMatrixZ(S16(r + 4), m);
    return m;
}

gptr DC2CALL ScaleMatrix(gptr m, gptr v)                             /* 0x4C10E0 */
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            M(m, i, j) = (s16)((s32)((u32)M(m, i, j) * U32(v + 4 * j)) >> 12);
    return m;
}

gptr DC2CALL TransposeMatrix(gptr m, gptr out)                       /* 0x4C1170 */
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            U16(out + 2 * (i * 3 + j)) = U16(m + 2 * (j * 3 + i));
    return out;
}

void DC2CALL OuterProduct12(gptr a, gptr b, gptr out)                /* 0x4C11C0 */
{
    /* plain cross product of VECTORs (no shift); the original also writes
       stack garbage into out+12 (pad) */
    u32 ax = U32(a), ay = U32(a + 4), az = U32(a + 8);
    u32 bx = U32(b), by = U32(b + 4), bz = U32(b + 8);
    U32(out) = bz * ay - by * az;
    U32(out + 4) = bx * az - ax * bz;
    U32(out + 8) = ax * by - bx * ay;
}

/* ---- perspective ------------------------------------------------------------ */
s32 DC2CALL Gte_DepthCue(s32 sz)                                     /* 0x4C1310 */
{
    s32 nearz = S32(GTE_DQNEAR), farz = S32(GTE_DQFAR);
    if (sz < nearz) return 0;
    if (sz > farz) return 0x1000;
    return ftol((long double)(s32)(sz - nearz) / (long double)(s32)(farz - nearz) * (long double)4096.0f);
}

/* MAC1..3 -> screen x, y (floats) and z */
void DC2CALL Gte_Project(gptr sx, gptr sy, gptr sz)                  /* 0x4C1350 */
{
    s32 x = S32(GTE_MAC1), y = S32(GTE_MAC1 + 4), z = S32(GTE_MAC1 + 8);
    s32 h = S32(GTE_H), h2 = S32(GTE_ZNEAR);
    s32 hx = (s32)((u32)h * (u32)x), hy = (s32)((u32)y * (u32)h);
    if (z >= h2) {
        F32(sx) = (float)((long double)hx / z + S32(GTE_OFX));
        F32(sy) = (float)((long double)hy / z + S32(GTE_OFY));
        S32(sz) = z;
    } else if (z <= 0) {                     /* behind the camera: clamp to H/2 */
        F32(sx) = (float)((long double)hx / h2 + S32(GTE_OFX));
        F32(sy) = (float)((long double)hy / h2 + S32(GTE_OFY));
        S32(sz) = 0;
        U32(GTE_FLAG) |= 0x80000000u;
    } else {                                 /* 0 < z < H/2 */
        long double k = (long double)z / h2;
        float ty = (float)((long double)y * k);
        F32(sx) = (float)((long double)x * k * h / z + S32(GTE_OFX));
        F32(sy) = (float)((long double)h * ty / z + S32(GTE_OFY));
        S32(sz) = z;
    }
}

/* MAC = (RT * v) >> 12 + TR */
static void rt_tr(gptr v)
{
    ApplyMatrix(GTE_RT, v, GTE_MAC1);
    S32(GTE_MAC1) += S32(GTE_TR);
    S32(GTE_MAC1 + 4) += S32(GTE_TR + 4);
    S32(GTE_MAC1 + 8) += S32(GTE_TR + 8);
}

static void push_sxy(void)
{
    U32(GTE_SXY + 0) = U32(GTE_SXY + 8);  U32(GTE_SXY + 4) = U32(GTE_SXY + 12);
    U32(GTE_SXY + 8) = U32(GTE_SXY + 16); U32(GTE_SXY + 12) = U32(GTE_SXY + 20);
}

static void push_sz(void)
{
    u32 a = U32(GTE_SZ_A);
    U32(GTE_SZ_C) = U32(GTE_SZ_B);
    U32(GTE_SZ_A) = U32(GTE_SZ3);
    U32(GTE_SZ_B) = a;
}

s32 DC2CALL RotTransPers(gptr v, gptr sxy, gptr p, gptr flag)        /* 0x4C1220 */
{
    (void)flag;
    U32(GTE_FLAG) = 0;
    rt_tr(v);
    push_sz();
    push_sxy();
    Gte_Project(GTE_SXY + 16, GTE_SXY + 20, GTE_SZ3);
    S32(p) = Gte_DepthCue(S32(GTE_SZ3));
    U32(sxy) = U32(GTE_SXY + 16);
    U32(sxy + 4) = U32(GTE_SXY + 20);
    return S32(GTE_SZ3) >> 2;
}

s32 DC2CALL RotTransPers3(gptr v0, gptr v1, gptr v2, gptr sxy0, gptr sxy1, gptr sxy2,
                          gptr p, gptr flag)                         /* 0x4C1470 */
{
    (void)flag;
    U32(GTE_FLAG) = 0;
    U32(GTE_SZ_C) = U32(GTE_SZ3);
    rt_tr(v0); Gte_Project(sxy0, sxy0 + 4, GTE_SZ_B);
    rt_tr(v1); Gte_Project(sxy1, sxy1 + 4, GTE_SZ_A);
    rt_tr(v2); Gte_Project(sxy2, sxy2 + 4, GTE_SZ3);
    S32(p) = Gte_DepthCue(S32(GTE_SZ3));
    return S32(GTE_SZ3) >> 2;
}

s32 DC2CALL RotTransPers4(gptr v0, gptr v1, gptr v2, gptr v3, gptr sxy0, gptr sxy1,
                          gptr sxy2, gptr sxy3, gptr p, gptr flag)   /* 0x4C15D0 */
{
    (void)flag;
    U32(GTE_FLAG) = 0;
    rt_tr(v0); Gte_Project(sxy0, sxy0 + 4, GTE_SZ_C);
    rt_tr(v1); Gte_Project(sxy1, sxy1 + 4, GTE_SZ_B);
    rt_tr(v2); Gte_Project(sxy2, sxy2 + 4, GTE_SZ_A);
    rt_tr(v3); Gte_Project(sxy3, sxy3 + 4, GTE_SZ3);
    S32(p) = Gte_DepthCue(S32(GTE_SZ3));
    return S32(GTE_SZ3) >> 2;
}

/* ---- normals and colour ----------------------------------------------------- */
static void normal1(gptr in, gptr out)
{
    float x = (float)S16(in), y = (float)S16(in + 2);
    long double z = S16(in + 4);
    long double len = sqrtl(z * z + (long double)y * y + (long double)x * x);
    if (len == 0.0L) len = 1.0L;
    S16(out) = (s16)ftol((long double)x * 4096.0 / len);
    S16(out + 2) = (s16)ftol((long double)y * 4096.0 / len);
    S16(out + 4) = (s16)ftol(z * 4096.0 / len);
}

void DC2CALL VectorNormalS(gptr in, gptr out)                        /* 0x4C1790: two SVECTORs */
{
    normal1(in, out);
    normal1(in + 6, out + 6);
}

void DC2CALL Gte_ClampIR(gptr v, gptr sv)                            /* 0x4C19B0: 0..0x7FFF */
{
    for (int i = 0; i < 3; i++) {
        s32 x = S32(v + 4 * i);
        S16(sv + 2 * i) = x < 0 ? 0 : x > 0x7FFF ? 0x7FFF : (s16)x;
    }
}

void DC2CALL Gte_NormalColor(gptr v, gptr rgb, gptr out)             /* 0x4C18E0 */
{
    gptr t = Tmp_Push(0x18), lv = t, sv = t + 0x10;
    ApplyMatrix(GTE_LLM, v, lv);
    Gte_ClampIR(lv, sv);
    ApplyMatrix(GTE_LCM, sv, lv);
    for (int i = 0; i < 3; i++) {
        s32 c = (s32)((u32)S32(lv + 4 * i) + (u32)S16(GTE_BK + 2 * i));
        s32 x = (s32)((u32)U8(rgb + i) * (u32)c) >> 12;
        U8(out + i) = x > 0xFF ? 0xFF : (u8)x;
    }
    Tmp_Pop(0x18);
}

void DC2CALL SetLightMatrix(gptr m) { memcpy(MEM(GTE_LLM), MEM(m), 32); }   /* 0x4C1A30 */
void DC2CALL SetColorMatrix(gptr m) { memcpy(MEM(GTE_LCM), MEM(m), 32); }   /* 0x4C1A50 */

void DC2CALL LoadAverageShort0(gptr v0, gptr v1, s32 p0, s32 p1, gptr out)  /* 0x4C1A70 */
{
    for (int i = 0; i < 3; i++)
        S16(out + 2 * i) = (s16)((s32)((u32)(S16(v0 + 2 * i) * p0) + (u32)(S16(v1 + 2 * i) * p1)) >> 12);
}

void DC2CALL LoadAverageCol(gptr v0, gptr v1, s32 p0, s32 p1, gptr out)     /* 0x4C1AD0 */
{
    for (int i = 0; i < 3; i++)
        U8(out + i) = (u8)((s32)((u32)(U8(v0 + i) * p0) + (u32)(U8(v1 + i) * p1)) >> 12);
}

/* ---- register loads ------------------------------------------------------- */
void DC2CALL SetRotMatrix(gptr m) { memcpy(MEM(GTE_RT), MEM(m), 20); }      /* 0x4C1B40 */

void DC2CALL SetTransMatrix(gptr m)                                  /* 0x4C1B60 */
{
    U32(GTE_TR) = U32(m + 0x14); U32(GTE_TR + 4) = U32(m + 0x18); U32(GTE_TR + 8) = U32(m + 0x1C);
}

void DC2CALL SetDepthScale(float f) { F32(GTE_ZSCALE) = (float)((long double)f + f); } /* 0x4C1B90 */

void DC2CALL gte_ldclmv(gptr m)                                      /* 0x4C1BA0: column -> IR */
{
    U16(GTE_IR1) = U16(m); U16(GTE_IR1 + 2) = U16(m + 6); U16(GTE_IR1 + 4) = U16(m + 12);
}

void DC2CALL gte_ldrgb(gptr c) { U32(GTE_RGBC) = U32(c); }           /* 0x4C1BD0 */

void DC2CALL gte_ldsxy3(gptr a, gptr b, gptr c)                      /* 0x4C1BE0 */
{
    U32(GTE_SXY) = U32(a);      U32(GTE_SXY + 4) = U32(a + 4);
    U32(GTE_SXY + 8) = U32(b);  U32(GTE_SXY + 12) = U32(b + 4);
    U32(GTE_SXY + 16) = U32(c); U32(GTE_SXY + 20) = U32(c + 4);
}

void DC2CALL gte_ldv0(gptr v) { U32(GTE_V0) = U32(v); U32(GTE_V0 + 4) = U32(v + 4); }   /* 0x4C1C20 */

void DC2CALL gte_ldv3(gptr v0, gptr v1, gptr v2)                     /* 0x4C1C40 */
{
    U32(GTE_V0) = U32(v0); U32(GTE_V0 + 4) = U32(v0 + 4);
    U32(GTE_V1) = U32(v1); U32(GTE_V1 + 4) = U32(v1 + 4);
    U32(GTE_V2) = U32(v2); U32(GTE_V2 + 4) = U32(v2 + 4);
}

void DC2CALL gte_ldv3c(gptr v)                                       /* 0x4C1C80 */
{
    U32(GTE_V0) = U32(v);      U32(GTE_V0 + 4) = U32(v + 4);
    U32(GTE_V1) = U32(v + 8);  U32(GTE_V1 + 4) = U32(v + 12);
    U32(GTE_V2) = U32(v + 16); U32(GTE_V2 + 4) = U32(v + 20);
}

/* ---- operations ------------------------------------------------------------ */
void DC2CALL gte_nccs(void) { Gte_NormalColor(GTE_V0, GTE_RGBC, GTE_RGB2); }   /* 0x4C1CC0 */

void DC2CALL gte_ncct(void)                                          /* 0x4C1CE0 */
{
    Gte_NormalColor(GTE_V0, GTE_RGBC, GTE_RGB0);
    Gte_NormalColor(GTE_V1, GTE_RGBC, GTE_RGB1);
    Gte_NormalColor(GTE_V2, GTE_RGBC, GTE_RGB2);
}

void DC2CALL gte_nclip(void)                                         /* 0x4C1D20 */
{
    long double a = ((long double)F32(GTE_SXY + 20) - F32(GTE_SXY + 4)) *
                    ((long double)F32(GTE_SXY + 8) - F32(GTE_SXY));
    long double b = ((long double)F32(GTE_SXY + 12) - F32(GTE_SXY + 4)) *
                    ((long double)F32(GTE_SXY + 16) - F32(GTE_SXY));
    S32(GTE_MAC0) = ftol(a - b);
}

static void rt_ir(gptr v)
{
    ApplyMatrix(GTE_RT, v, GTE_MAC1);
    U16(GTE_IR1) = U16(GTE_MAC1); U16(GTE_IR1 + 2) = U16(GTE_MAC1 + 4); U16(GTE_IR1 + 4) = U16(GTE_MAC1 + 8);
}

void DC2CALL gte_rtv0(void) { rt_ir(GTE_V0); }                       /* 0x4C1D70 */
void DC2CALL gte_rtv1(void) { rt_ir(GTE_V1); }                       /* 0x4C1DB0 */
void DC2CALL gte_rtv2(void) { rt_ir(GTE_V2); }                       /* 0x4C1DF0 */
void DC2CALL gte_rtir(void) { rt_ir(GTE_IR1); }                      /* 0x4C1E30 */

void DC2CALL gte_rtps(void)                                          /* 0x4C1E70 */
{
    U32(GTE_FLAG) = 0;
    rt_tr(GTE_V0);
    push_sz();
    push_sxy();
    Gte_Project(GTE_SXY + 16, GTE_SXY + 20, GTE_SZ3);
    S32(GTE_IR0) = Gte_DepthCue(S32(GTE_SZ3));
}

void DC2CALL gte_rtpt(void)                                          /* 0x4C1F40 */
{
    U32(GTE_FLAG) = 0;
    U32(GTE_SZ_C) = U32(GTE_SZ3);
    rt_tr(GTE_V0); Gte_Project(GTE_SXY, GTE_SXY + 4, GTE_SZ_B);
    rt_tr(GTE_V1); Gte_Project(GTE_SXY + 8, GTE_SXY + 12, GTE_SZ_A);
    rt_tr(GTE_V2); Gte_Project(GTE_SXY + 16, GTE_SXY + 20, GTE_SZ3);
    S32(GTE_IR0) = Gte_DepthCue(S32(GTE_SZ3));
}

/* ---- register stores ------------------------------------------------------- */
void DC2CALL gte_stclmv(gptr m)                                      /* 0x4C2090 */
{
    U16(m) = U16(GTE_IR1); U16(m + 6) = U16(GTE_IR1 + 2); U16(m + 12) = U16(GTE_IR1 + 4);
}
void DC2CALL gte_stlvnl(gptr v)                                      /* 0x4C20C0 */
{
    U32(v) = U32(GTE_MAC1); U32(v + 4) = U32(GTE_MAC1 + 4); U32(v + 8) = U32(GTE_MAC1 + 8);
}
void DC2CALL gte_stopz(gptr p) { U32(p) = U32(GTE_MAC0); }           /* 0x4C20E0 */
void DC2CALL gte_stflg(gptr p) { U32(p) = U32(GTE_FLAG); }           /* 0x4C20F0 */
void DC2CALL gte_strgb(gptr p) { U32(p) = U32(GTE_RGB2); }           /* 0x4C2100 */
void DC2CALL gte_strgb3(gptr p)                                      /* 0x4C2110 */
{
    U32(p) = U32(GTE_RGB0); U32(p + 4) = U32(GTE_RGB1); U32(p + 8) = U32(GTE_RGB2);
}
void DC2CALL gte_strgb3_gt3(gptr p)                                  /* 0x4C2130 */
{
    U32(p + 4) = U32(GTE_RGB0); U32(p + 0x18) = U32(GTE_RGB1); U32(p + 0x2C) = U32(GTE_RGB2);
}
void DC2CALL gte_stsv(gptr p) { U32(p) = U32(GTE_IR1); U32(p + 4) = U32(GTE_IR1 + 4); }     /* 0x4C2170 */
void DC2CALL gte_stsxy(gptr p) { U32(p) = U32(GTE_SXY + 16); U32(p + 4) = U32(GTE_SXY + 20); } /* 0x4C2190 */
void DC2CALL gte_stsxy3c(gptr p) { memcpy(MEM(p), MEM(GTE_SXY), 24); }                   /* 0x4C21B0 */

static void stsxy3(gptr p, u32 o0, u32 o1, u32 o2)
{
    U32(p + o0) = U32(GTE_SXY);      U32(p + o0 + 4) = U32(GTE_SXY + 4);
    U32(p + o1) = U32(GTE_SXY + 8);  U32(p + o1 + 4) = U32(GTE_SXY + 12);
    U32(p + o2) = U32(GTE_SXY + 16); U32(p + o2 + 4) = U32(GTE_SXY + 20);
}
void DC2CALL gte_stsxy3_g3(gptr p) { stsxy3(p, 0x08, 0x18, 0x28); }  /* 0x4C21F0 */
void DC2CALL gte_stsxy3_gt3(gptr p) { stsxy3(p, 0x08, 0x1C, 0x30); } /* 0x4C2230 */

void DC2CALL gte_stsz(gptr p) { U32(p) = U32(GTE_SZ3); }             /* 0x4C22B0 */
void DC2CALL gte_stsz3(gptr p)                                       /* 0x4C22C0 */
{
    U32(p) = U32(GTE_SZ_B); U32(p + 4) = U32(GTE_SZ_A); U32(p + 8) = U32(GTE_SZ3);
}
void DC2CALL gte_stsz4(gptr p)                                       /* 0x4C22E0 */
{
    U32(p) = U32(GTE_SZ_C); U32(p + 4) = U32(GTE_SZ_B); U32(p + 8) = U32(GTE_SZ_A); U32(p + 12) = U32(GTE_SZ3);
}

/* z / ZSCALE as float into the depth fields of PC primitives */
static float zf(u32 reg) { return (float)((long double)S32(reg) / F32(GTE_ZSCALE)); }

void DC2CALL PC_stsz4_g4(gptr p)                                     /* 0x4C2310 */
{
    F32(p + 0x10) = zf(GTE_SZ_C); F32(p + 0x20) = zf(GTE_SZ_B);
    F32(p + 0x30) = zf(GTE_SZ_A); F32(p + 0x40) = zf(GTE_SZ3);
}
void DC2CALL PC_stsz_g4_flat(gptr p)                                 /* 0x4C2360 */
{
    float z = zf(GTE_SZ3);
    F32(p + 0x10) = z; F32(p + 0x20) = z; F32(p + 0x30) = z; F32(p + 0x40) = z;
}
void DC2CALL PC_stsz3_g3(gptr p)                                     /* 0x4C2390 */
{
    F32(p + 0x10) = zf(GTE_SZ_B); F32(p + 0x20) = zf(GTE_SZ_A); F32(p + 0x30) = zf(GTE_SZ3);
}
void DC2CALL PC_stsz3_gt3(gptr p)                                    /* 0x4C2420 */
{
    F32(p + 0x10) = zf(GTE_SZ_B); F32(p + 0x24) = zf(GTE_SZ_A); F32(p + 0x38) = zf(GTE_SZ3);
}
void DC2CALL PC_stsz4_gt4(gptr p)                                    /* 0x4C2460 */
{
    F32(p + 0x10) = zf(GTE_SZ_C); F32(p + 0x24) = zf(GTE_SZ_B);
    F32(p + 0x38) = zf(GTE_SZ_A); F32(p + 0x4C) = zf(GTE_SZ3);
}
void DC2CALL gte_stszotz(gptr p) { S32(p) = S32(GTE_SZ3) >> 2; }     /* 0x4C24B0 */

/* ---- a polygon clipped at the near plane (PC only) ---------------------------
 * n SVECTORs v (3: in the order 0 1 2, 4: 0 1 3 2) with their u, v bytes uv
 * through RT and TR; the edges cut at z = GTE_ZNEAR (H / 2, so a cut point
 * projects as 2x, 2y). out: {float x, y, z; u8 u, v; 2 bytes kept} for each
 * vertex left (up to n + 1); nclip (when not 0 and 3 or more are left): the
 * winding of the first, second and last of them. Returns the count. */
static void clip_xform(gptr v, s32 *x, s32 *y, s32 *z)
{
    s32 a = S16(v), b = S16(v + 2), c = S16(v + 4);
    *x = (s32)((u32)(dot3(GTE_RT, 0, a, b, c) >> 12) + U32(GTE_TR));
    *y = (s32)((u32)(dot3(GTE_RT, 1, a, b, c) >> 12) + U32(GTE_TR + 4));
    *z = (s32)((u32)(dot3(GTE_RT, 2, a, b, c) >> 12) + U32(GTE_TR + 8));
}

static void clip_put(gptr o, s32 x, s32 y, s32 z, u8 u, u8 v)
{
    long double lz = (long double)z;
    F32(o + 8) = (float)lz;
    F32(o) = (float)((long double)(s32)((u32)S32(GTE_H) * (u32)x) / lz + (long double)S32(GTE_OFX));
    F32(o + 4) = (float)((long double)(s32)((u32)S32(GTE_H) * (u32)y) / lz + (long double)S32(GTE_OFY));
    U8(o + 0xC) = u; U8(o + 0xD) = v;
}

s32 DC2CALL PC_ClipProject(gptr v, gptr uv, gptr out, gptr nclip, s32 n) /* 0x4C24C0 */
{
    static const u8 order4[4] = {1, 3, 2, 0}, order3[4] = {1, 2, 0, 0};
    const u8 *ord = n == 4 ? order4 : order3;
    s32 near = S32(GTE_ZNEAR), cnt = 0;
    s32 fx, fy, fz, px, py, pz;
    clip_xform(v, &fx, &fy, &fz);
    px = fx; py = fy; pz = fz;
    u8 fu = U8(uv), fv = U8(uv + 1), pu = fu, pv = fv;
    s32 fbehind = fz < near, pbehind = fbehind;
    gptr o = out;
    for (s32 i = 0; i < n; i++) {
        s32 cx, cy, cz, cbehind;
        u8 cu, cv;
        if (i == 3) { cx = fx; cy = fy; cz = fz; cu = fu; cv = fv; cbehind = fbehind; }
        else {
            u32 k = ord[i];
            clip_xform(v + k * 8, &cx, &cy, &cz);
            cu = U8(uv + k * 2); cv = U8(uv + k * 2 + 1);
            cbehind = cz < near;
        }
        if (pbehind + cbehind == 0) {               /* both in front: the previous one */
            clip_put(o, px, py, pz, pu, pv);
            cnt++; o += 0x10;
        } else if (pbehind + cbehind == 1) {        /* crossing: the cut point (after the previous one when it is in front) */
            long double t = (long double)(s32)(pz - near) / (long double)(s32)(pz - cz);
            float X = (float)((long double)px - (long double)(s32)(px - cx) * t);
            float Y = (float)((long double)py - (long double)(s32)(py - cy) * t);
            float U = (float)((long double)pu - (long double)((s32)pu - cu) * t);
            long double V = (long double)pv - (long double)((s32)pv - cv) * t;
            if (!pbehind) { clip_put(o, px, py, pz, pu, pv); cnt++; o += 0x10; }
            F32(o + 8) = (float)(long double)near;
            F32(o) = (float)((long double)X + X + (long double)S32(GTE_OFX));
            F32(o + 4) = (float)((long double)Y + Y + (long double)S32(GTE_OFY));
            U8(o + 0xC) = (u8)ftol(U);
            U8(o + 0xD) = (u8)ftol(V);
            cnt++; o += 0x10;
        }
        px = cx; py = cy; pz = cz; pu = cu; pv = cv; pbehind = cbehind;
    }
    if (nclip && cnt >= 3) {
        gptr l = out + (u32)cnt * 0x10 - 0x10;
        long double x0 = F32(out), y0 = F32(out + 4);
        S32(nclip) = ftol(((long double)F32(l + 4) - y0) * ((long double)F32(out + 0x10) - x0)
                          - ((long double)F32(out + 0x14) - y0) * ((long double)F32(l) - x0));
    }
    return cnt;
}
