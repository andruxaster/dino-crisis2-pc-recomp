/* Room script built-ins that work on game objects:
 * obj_select (12), obj_set (1E), obj_get (1F), obj44_init (44),
 * obj_enable (45), wait_frame (2A), cutscene_camera (4A).
 * The selected object is thread+0x9E (kind) / +0x9F (index, signed).
 * Kinds: 0 objects G+0x940 (0x260 bytes), 1 effects G+0x93C (0x70, index+12),
 * 2 G+0x944 (0xAC), 3 G+0x948 (0x34, index+1), 4/7 nothing, 5 globals,
 * 6 camera zones G+0xA44 (0x2C), 8 collision polygons G+0xA4C. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define SPV(t, i)  (U32(t + T_SP) + 4 * (i))        /* address of stack slot i */
#define SEL(t)     ((s32)S8(t + T_SEL1))

static gptr obj_addr(gptr t)  { return U32(G() + 0x940) + SEL(t) * 0x260; }
static gptr eff_addr(gptr t)  { return U32(G() + 0x93C) + (SEL(t) + 12) * 0x70; }
static gptr k2_addr(gptr t)   { return U32(G() + 0x944) + SEL(t) * 0xAC; }
static gptr k3_addr(gptr t)   { return U32(G() + 0x948) + (SEL(t) + 1) * 0x34; }
static gptr zone_addr(gptr t) { return U32(G() + 0xA44) + SEL(t) * 0x2C; }
static gptr poly_addr(gptr t) { gptr c = U32(G() + 0xA4C); return c + U32(c + SEL(t) * 4 + 8); }

/* ---- helpers on objects ---------------------------------------------------- */
void DC2CALL Obj_SyncLinks(gptr obj)                               /* 0x48FC70 */
{
    if (!U8(obj + 0x54)) return;
    gptr p = U32(obj + 0x64);
    do {
        U16(p + 0x6C) = U16(p + 0x40);
        U16(p + 0x6E) = U16(p + 0x42);
        U16(p + 0x70) = U16(p + 0x44);
        p = U32(p + 0x54);
    } while (p);
}

void DC2CALL Obj_SavePos(gptr obj)                                 /* 0x48FC40 */
{
    U16(obj + 0xE0) = U16(obj + 0x40);
    U16(obj + 0xE2) = U16(obj + 0x42);
    U16(obj + 0xE4) = U16(obj + 0x44);
    Obj_SyncLinks(obj);
}

void DC2CALL Obj_Free(gptr obj)                                    /* 0x48D890 */
{
    gptr p = U32(obj + 0x64);
    U32(obj + 0x64) = 0;
    for (; p; p = U32(p + 0x54)) U32(p + 0x50) = 0;     /* release linked parts */
}

/* request a camera change; the camera task loads it (0x403A50) */
void DC2CALL Camera_Switch(u32 cam)
{
    gptr g = G();
    U8(g + 0x7C6) = U8(g + 0x7C4);           /* previous */
    U8(g + 0x7C4) = (u8)cam;
    U8(g + 0x84A) = (u8)cam;
    if (!U8(g + 0xB49)) Flag_Set(2, 0x18, 1);
    Flag_Set(2, 1, 0);
    Flag_Set(2, 0xD, 0);
    Flag_Set(2, 2, 1);
}

/* object matrix: rotation (+0x48) and scale (+0x6C) into +0x20; then each
 * linked part: own rotation, joint offset (+0x64) rotated by the parent's
 * matrix and added to the parent's position, matrix = parent * own. */
void DC2CALL Obj_UpdateMatrix(gptr obj)                            /* 0x48FDE0 */
{
    RotMatrixYXZ(obj + 0x48, obj + 0x20);
    ScaleMatrix(obj + 0x20, obj + 0x6C);
    gptr p = U32(obj + 0x64);
    do {
        gptr parent = U32(p + 0x58);
        RotMatrixYXZ(p + 0x48, p + 0x20);
        SetRotMatrix(parent + 0x20);
        gte_ldv0(p + 0x64);
        gte_rtv0();
        gte_stsv(p + 0x40);
        for (int c = 0; c < 3; c++) {                  /* columns: parent * own */
            gte_ldclmv(p + 0x20 + 2 * c);
            gte_rtir();
            U16(p + 0x40 + 2 * c) += U16(parent + 0x40 + 2 * c);
            gte_stclmv(p + 0x20 + 2 * c);
        }
        p = U32(p + 0x54);
    } while (p);
}

/* eye = target + RotMatrixYXZ(angles) * -dist (0x403840) */
void DC2CALL Camera_LookAt(gptr target, gptr dist, gptr angles, gptr eye)
{
    gptr a = Tmp_Push(8 + 0x20), m = a + 8;
    S16(a) = S16(angles); S16(a + 2) = S16(angles + 2); S16(a + 4) = S16(angles + 4);
    RotMatrixYXZ(a, m);
    S16(a) = (s16)-S16(dist); S16(a + 2) = (s16)-S16(dist + 2); S16(a + 4) = (s16)-S16(dist + 4);
    ApplyMatrixSV(m, a, a);
    S16(eye) = (s16)(S16(target) + S16(a));
    S16(eye + 2) = (s16)(S16(target + 2) + S16(a + 2));
    S16(eye + 4) = (s16)(S16(target + 4) + S16(a + 4));
    Tmp_Pop(8 + 0x20);
}

void DC2CALL Obj_Disable(gptr obj)                                 /* 0x48D800 */
{
    if (U8(obj + 0x50) & 1) {
        U32(obj + 0x50) = 0;
        Obj_Free(obj);
    }
}

/* ---- obj_set: value = sp[0], parameter = sp[1] ------------------------------ */
static void set_obj(gptr t)                                         /* 0x487C80 */
{
    gptr o = obj_addr(t), v = SPV(t, 0);
    u32 p = U32(SPV(t, 1));
    switch (p) {
    case 0: U8(o + 0x54) = U8(v); break;
    case 1: U8(o + 0x55) = U8(v); break;
    case 2: U8(o + 0x56) = U8(v); break;
    case 3: U8(o + 0x57) = U8(v); break;
    case 4: U16(o + 0xE0) = U16(v); U16(o + 0x40) = U16(v); break;
    case 5: U16(o + 0xE2) = U16(v); U16(o + 0x42) = U16(v);
            S16(o + 0x5A) = (s16)(S16(v) / -1000); break;      /* floor level */
    case 6: U16(o + 0xE4) = U16(v); U16(o + 0x44) = U16(v); break;
    case 7: U16(o + 0x48) = U16(v); break;
    case 8: U16(o + 0x4A) = U16(v); break;
    case 9: U16(o + 0x4C) = U16(v); break;
    case 10: U16(o + 0x10C) = U16(v); break;
    case 11: U16(o + 0xF0) = U16(v); break;
    case 12: U16(o + 0xF2) = U16(v); break;
    case 13: U16(o + 0xF4) = U16(v); break;
    case 14: U32(o + 0x50) = U32(v); break;
    case 15: case 16: case 17: case 18: case 19:
    case 20: case 21: case 22: case 23: case 24: U16(o + 0x1A2 + 2 * p) = U16(v); break;
    case 31: U16(o + 0x5A) = U16(v); break;
    case 32: U16(o + 0x108) = U16(v); break;
    case 33: U32(o + 0x74) = U32(v); U32(o + 0x70) = U32(v); U32(o + 0x6C) = U32(v); break;
    case 34: U8(o + 0x118) = U8(v); break;
    case 35: U8(o + 0x11B) = U8(v); break;
    case 36: U32(o + 0x88) = U32(v); break;
    case 37: U8(o + 0xFE) = U8(v); break;
    case 38: U16(o + 0x102) = U16(v); break;
    case 39: U16(o + 0x174) = U16(v); break;
    default: break;
    }
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

static void set_eff(gptr t)                                         /* 0x487FF0 */
{
    gptr o = eff_addr(t), v = SPV(t, 0);
    switch (U32(SPV(t, 1))) {
    case 0: U8(o + 0x46) = U8(v); break;
    case 1: U8(o + 0x47) = U8(v); break;
    case 4: U16(o + 0x40) = U16(v); break;
    case 5: U16(o + 0x42) = U16(v); break;
    case 6: U16(o + 0x44) = U16(v); break;
    case 7: U16(o + 0x48) = U16(v); break;
    case 8: U16(o + 0x4A) = U16(v); break;
    case 9: U16(o + 0x4C) = U16(v); break;
    case 10: U8(o + 0x4F) = U8(v); break;
    case 11: U16(o + 0x60) = U16(v); break;
    case 12: U16(o + 0x62) = U16(v); break;
    case 13: U16(o + 0x64) = U16(v); break;
    case 14: U32(o + 0x50) = U32(v); break;
    case 30: U16(o + 0x6C) = U16(v); break;
    case 31: U16(o + 0x6E) = U16(v); break;
    default: break;
    }
}

static void set_k2(gptr t)                                          /* 0x488120 */
{
    gptr o = k2_addr(t), v = SPV(t, 0);
    u32 p = U32(SPV(t, 1));
    switch (p) {
    case 0: U8(o + 0x54) = U8(v); break;
    case 4: U16(o + 0x40) = U16(v); break;
    case 5: U16(o + 0x42) = U16(v); break;
    case 6: U16(o + 0x44) = U16(v); break;
    case 7: U16(o + 0x48) = U16(v); break;
    case 8: U16(o + 0x4A) = U16(v); break;
    case 9: U16(o + 0x4C) = U16(v); break;
    case 10: U8(o + 0x7A) = U8(v); break;
    case 14: U32(o + 0x50) = U32(v); break;
    case 15: case 16: case 17: case 18: case 19:
    case 20: case 21: case 22: case 23: case 24: U16(o + 0x7A + 2 * p) = U16(v); break;
    case 30: U32(o + 0x70) = U32(v); break;
    case 31: U16(o + 0x68) = U16(v); break;
    case 32: U16(o + 0x6A) = U16(v); break;
    case 33: U16(o + 0x6E) = U16(v); U16(o + 0x6C) = U16(v); break;
    case 34: U8(o + 0x64) = U8(v); break;
    case 35: U8(o + 0x65) = U8(v); break;
    case 36: U8(o + 0x66) = U8(v); break;
    case 37: U16(o + 0x6C) = U16(v); break;
    case 38: U16(o + 0x6E) = U16(v); break;
    case 39: U8(o + 0x84) = U8(v); break;
    default: break;
    }
}

static void set_k3(gptr t)                                          /* 0x4882A0 */
{
    gptr o = k3_addr(t), v = SPV(t, 0);
    u32 p = U32(SPV(t, 1));
    switch (p) {
    case 0: U8(o + 3) = U8(v); break;
    case 10: U8(o + 2) = U8(v); break;
    case 14: U8(o + 0) = U8(v); break;
    case 15: case 16: case 17: case 18: case 19:
    case 20: case 21: case 22: case 23: case 24: U16(o + 2 * p - 0x12) = U16(v); break;
    case 25: U8(o + 1) = U8(v); break;
    default: break;
    }
}

/* after changing camera position/angles: rebuild the view (0x488340) */
static void camera_rebuild(void)
{
    gptr cam = G() + 0x778;
    Camera_LookAt(cam + 0x28, cam + 0x30, cam + 0x20, cam + 0x38);
    U16(cam + 0x20) &= 0xFFF; U16(cam + 0x22) &= 0xFFF; U16(cam + 0x24) &= 0xFFF;
    gptr v = Tmp_Push(8);
    U16(v + 0) = (u16)-U16(cam + 0x20);
    U16(v + 2) = (u16)-U16(cam + 0x22);
    U16(v + 4) = U16(cam + 0x24);
    RotMatrix(v, cam);
    Tmp_Pop(8);
}

static void set_global(gptr t)                                      /* 0x4883B0 */
{
    gptr g = G(), v = SPV(t, 0);
    switch (U32(SPV(t, 1)) - 33) {            /* parameters 33..72, same numbers as obj_get */
    case 0: U32(0x87F940) = U32(v); break;
    case 2: U8(g + 0xB4B) = U8(v);
            SetDepthScale((float)(s32)(0x400u << (U8(G() + 0xB4B) & 31))); break;
    case 4: U8(g + 0xB58) = U8(v); break;
    case 5: U8(g + 0xB59) = U8(v); break;
    case 6: U8(g + 0xB5A) = U8(v); break;
    case 20: U8(g + 0xB5B) = U8(v); break;
    case 33: U8(g + 0xB5C) = U8(v); break;
    case 34: U8(g + 0xB5D) = U8(v); break;
    case 7: U16(g + 0x7B0) = U16(v); camera_rebuild(); break;   /* eye */
    case 8: U16(g + 0x7B2) = U16(v); camera_rebuild(); break;
    case 9: U16(g + 0x7B4) = U16(v); camera_rebuild(); break;
    case 10: U16(g + 0x7A0) = U16(v); camera_rebuild(); break;  /* target */
    case 11: U16(g + 0x7A2) = U16(v); camera_rebuild(); break;
    case 12: U16(g + 0x7A4) = U16(v); camera_rebuild(); break;
    case 13: U16(g + 0x798) = U16(v); camera_rebuild(); break;  /* angles */
    case 14: U16(g + 0x79A) = U16(v); camera_rebuild(); break;
    case 15: U16(g + 0x79C) = U16(v); camera_rebuild(); break;
    case 16: U16(g + 0x7A8) = U16(v); camera_rebuild(); break;
    case 17: U16(g + 0x7AA) = U16(v); camera_rebuild(); break;
    case 18: U16(g + 0x7AC) = U16(v); camera_rebuild(); break;
    case 19: U16(U32(g + 0x7B8) + 6) = U16(v);
             SetGeomScreen(U16(U32(G() + 0x7B8) + 6)); break; /* projection H */
    case 21: U8(g + 0x1204) = U8(v); break;
    case 22: U8(g + 0x1205) = U8(v); break;
    case 23: U8(g + 0x1206) = U8(v); break;
    case 24: U8(g + 0x1207) = U8(v); break;
    case 25: U32(g + 0xB54) = U32(v); break;
    case 26: U32(0x87DE00) = U32(v); break;
    case 28: U16(g + 0x1388) = U16(v); break;
    case 32: U8(g + 0x10AA) = U8(v); break;
    case 35: U16(0x87FC4A) = U16(v); break;                       /* script frame counter */
    case 36: Timer_Set(U32(v), 0); break;
    case 37: U8(g + 0x850) = U8(v); break;
    case 38: U8(g + 0x851) = U8(v); break;
    case 39: U16(g + 0x138A) = U16(v); break;
    default: break;
    }
}

static void set_zone(gptr t)                                        /* 0x4886F0 */
{
    gptr z = zone_addr(t), v = SPV(t, 0);
    switch (U32(SPV(t, 1))) {
    case 30: U8(z + 4) = U8(v); break;     /* camera */
    case 31: U8(z + 6) = U8(v); break;     /* level */
    case 32: U8(z + 5) = U8(v); break;
    default: break;
    }
}

static void set_poly(gptr t)                                        /* 0x488740 */
{
    if (U32(SPV(t, 1)) == 30) U8(poly_addr(t) + 3) = U8(SPV(t, 0));   /* ignore kinds */
}

static void set_none(gptr t) { (void)t; }                          /* 0x402520 */

/* ---- obj_get: parameter = sp[0]; unknown parameters return the thread address */
static s32 get_obj(gptr t)                                          /* 0x4887A0 */
{
    gptr o = obj_addr(t);
    u32 p = U32(SPV(t, 0));
    switch (p) {
    case 0: return U8(o + 0x54);
    case 1: return U8(o + 0x55);
    case 2: return U8(o + 0x56);
    case 3: return U8(o + 0x57);
    case 4: return S16(o + 0x40);
    case 5: return S16(o + 0x42);
    case 6: return S16(o + 0x44);
    case 7: return S16(o + 0x48);
    case 8: return S16(o + 0x4A);
    case 9: return S16(o + 0x4C);
    case 10: return U16(o + 0x10C);
    case 11: return S16(o + 0xF0);
    case 12: return S16(o + 0xF2);
    case 13: return S16(o + 0xF4);
    case 14: return S32(o + 0x50);
    case 15: case 16: case 17: case 18: case 19:
    case 20: case 21: case 22: case 23: case 24: return S16(o + 0x1A2 + 2 * p);
    case 30: return S32(o + 0x1C0);
    case 31: return S16(o + 0x5A);
    case 32: return S16(o + 0x108);
    case 33: return S32(o + 0x6C);
    case 34: return U8(o + 0x118);
    case 35: return U8(o + 0x11B);
    case 38: return U16(o + 0x102);
    case 39: return U16(o + 0x174);
    default: return (s32)t;
    }
}

static s32 get_eff(gptr t)                                          /* 0x488920 */
{
    gptr o = eff_addr(t);
    switch (U32(SPV(t, 0))) {
    case 0: return S8(o + 0x46);
    case 1: return S8(o + 0x47);
    case 4: return S16(o + 0x40);
    case 5: return S16(o + 0x42);
    case 6: return S16(o + 0x44);
    case 7: return S16(o + 0x48);
    case 8: return S16(o + 0x4A);
    case 9: return S16(o + 0x4C);
    case 10: return S8(o + 0x4F);
    case 11: return S16(o + 0x60);
    case 12: return S16(o + 0x62);
    case 13: return S16(o + 0x64);
    case 14: return S32(o + 0x50);
    case 30: return U16(o + 0x6C);
    case 31: return U16(o + 0x6E);
    default: return (s32)t;
    }
}

static s32 get_k2(gptr t)                                           /* 0x488A20 */
{
    gptr o = k2_addr(t);
    u32 p = U32(SPV(t, 0));
    switch (p) {
    case 0: return U8(o + 0x54);
    case 4: return S16(o + 0x40);
    case 5: return S16(o + 0x42);
    case 6: return S16(o + 0x44);
    case 10: return U8(o + 0x7A);
    case 14: return S32(o + 0x50);
    case 15: case 16: case 17: case 18: case 19:
    case 20: case 21: case 22: case 23: case 24: return S16(o + 0x7A + 2 * p);
    case 34: return U8(o + 0x64);
    case 35: return U8(o + 0x65);
    case 36: return U8(o + 0x66);
    case 37: return U16(o + 0x6C);
    case 38: return U16(o + 0x6E);
    case 39: return U8(o + 0x84);
    default: return (s32)t;
    }
}

static s32 get_k3(gptr t)                                           /* 0x488B20 */
{
    gptr o = k3_addr(t);
    u32 p = U32(SPV(t, 0));
    switch (p) {
    case 0: return S8(o + 3);
    case 10: return U8(o + 2);
    case 14: return U8(o + 0);
    case 15: case 16: case 17: case 18: case 19:
    case 20: case 21: case 22: case 23: case 24: return S16(o + 2 * p - 0x12);
    case 25: return U8(o + 1);
    default: return (s32)t;
    }
}

/* kind 4 (0x488BC0) returns whatever ECX held in the caller: no defined
 * value; no script uses it. Kind 7 (0x4D7290) returns 0. */
static s32 get_zero(gptr t) { (void)t; return 0; }

static s32 get_global(gptr t)                                       /* 0x488BD0 */
{
    gptr g = G();
    switch (U32(SPV(t, 0))) {
    case 30: return U16(g + 0x10A8);
    case 31: return U8(g + 0x7C6);
    case 32: return U16(g + 0x10B4);
    case 33: return S32(0x87F940);
    case 34: return U8(g + 0x108F);
    case 35: return U8(g + 0xB4B);
    case 36: return U8(g + 0x7C4);
    case 37: return U8(g + 0xB58);
    case 38: return U8(g + 0xB59);
    case 39: return U8(g + 0xB5A);
    case 40: return S16(g + 0x7B0);
    case 41: return S16(g + 0x7B2);
    case 42: return S16(g + 0x7B4);
    case 43: return S16(g + 0x7A0);
    case 44: return S16(g + 0x7A2);
    case 45: return S16(g + 0x7A4);
    case 46: return S16(g + 0x798);
    case 47: return S16(g + 0x79A);
    case 48: return S16(g + 0x79C);
    case 49: return S16(g + 0x7A8);
    case 50: return S16(g + 0x7AA);
    case 51: return U16(g + 0x7AC);
    case 52: return U16(U32(g + 0x7B8) + 6);
    case 53: return U8(g + 0xB5B);
    case 54: return U8(g + 0x1204);
    case 55: return U8(g + 0x1205);
    case 56: return U8(g + 0x1206);
    case 57: return U8(g + 0x1207);
    case 58: return S32(g + 0xB54);
    case 60: return S8(g + 0x108E);
    case 61: return S16(g + 0x1388);
    case 62: case 63: case 65: return S8(g + 0x10AA);
    case 66: return U8(g + 0xB5C);
    case 67: return S8(g + 0xB5D);
    case 68: return S16(0x87FC4A);
    case 69: return S32(g + 0x12C8);
    case 70: return U8(g + 0x850);
    case 71: return U8(g + 0x851);
    case 72: return S16(g + 0x138A);
    default: return (s32)t;
    }
}

static s32 get_zone(gptr t)                                         /* 0x488EE0 */
{
    gptr z = zone_addr(t);
    switch (U32(SPV(t, 0))) {
    case 30: return U8(z + 4);
    case 31: return U8(z + 6);
    case 32: return U8(z + 5);
    default: return (s32)t;
    }
}

static s32 get_poly(gptr t)                                         /* 0x488F30 */
{
    if (U32(SPV(t, 0)) == 30) return U8(poly_addr(t) + 3);
    return (s32)t;
}

static void (*const g_set[9])(gptr) = {set_obj, set_eff, set_k2, set_k3, set_none,
                                        set_global, set_zone, set_none, set_poly};
static s32 (*const g_get[9])(gptr) = {get_obj, get_eff, get_k2, get_k3, get_zero,
                                       get_global, get_zone, get_zero, get_poly};

/* ---- opcodes ---------------------------------------------------------------- */
u32 DC2CALL ScrOp_ObjSelect(gptr t)                                /* 12: obj_select(kind, index) */
{
    gptr sp = U32(t + T_SP);
    U8(t + T_SEL0) = U8(sp + 4);
    U8(t + T_SEL1) = U8(sp);
    U32(t + T_PC) += 2;
    return 0;
}

u32 DC2CALL ScrOp_ObjSet(gptr t)                                   /* 1E: obj_set(param, value) */
{
    g_set[U8(t + T_SEL0) % 9](t);
    U32(t + T_PC) += 2;
    return 0;
}

u32 DC2CALL ScrOp_ObjGet(gptr t)                                   /* 1F: r0 = obj_get(param) */
{
    s32 r = g_get[U8(t + T_SEL0) % 9](t);
    S32(t + T_REG) = r;
    Script_SetCond(t, r);
    U32(t + T_PC) += 2;
    return 0;
}

u32 DC2CALL ScrOp_EffectInit(gptr t)                              /* 13: 15 args -> effect */
{
    gptr sp = U32(t + T_SP);
    gptr e = U32(G() + 0x93C) + (U32(sp + 0x38) + 12) * 0x70;
    memset(MEM(e), 0, 0x70);
    U32(e + 0x50) = 7;
    U8(e + 0x4E) = U8(sp + 0x34);
    U16(e + 0x6C) = U16(sp + 0x30);
    U16(e + 0x6E) = U16(sp + 0x2C);
    U32(e + 0x68) = U32(sp + 0x28);
    U16(e + 0x40) = U16(sp + 0x24);
    U16(e + 0x42) = U16(sp + 0x20);
    U16(e + 0x44) = U16(sp + 0x1C);
    U16(e + 0x48) = U16(sp + 0x18);
    U16(e + 0x4A) = U16(sp + 0x14);
    U16(e + 0x4C) = U16(sp + 0x10);
    U32(t + T_PC) += 2;
    return 0;
}

u32 DC2CALL ScrOp_Obj14Init(gptr t)                                /* 14: 11 args -> G+0x944[i] */
{
    gptr sp = U32(t + T_SP);
    gptr e = U32(G() + 0x944) + U32(sp + 0x28) * 0xAC;
    memset(MEM(e), 0, 0xAC);
    U32(e + 0x50) = 1;
    U16(e + 0x6C) = 0x1E; U16(e + 0x6E) = 0x1E;
    U8(e + 0x67) = 0x2C;
    U8(e + 0x64) = 0x80; U8(e + 0x65) = 0x80; U8(e + 0x66) = 0x80;
    U8(e + 0x58) = U8(sp + 0x24);
    U16(e + 0x40) = U16(sp + 0x20);
    U16(e + 0x42) = U16(sp + 0x1C);
    U16(e + 0x44) = U16(sp + 0x18);
    U16(e + 0x98) = U16(sp + 0x14);
    U16(e + 0x9A) = U16(sp + 0x10);
    U16(e + 0x9C) = U16(sp + 0x0C);
    U16(e + 0x9E) = U16(sp + 0x08);
    U16(e + 0xA0) = U16(sp + 0x04);
    U16(e + 0xA2) = U16(sp + 0x00);
    U32(t + T_PC) += 2;
    return 0;
}

u32 DC2CALL ScrOp_WaitFrame(gptr t)                                /* 2A: wait until counter >= n */
{
    if ((s32)S16(0x87FC4A) < S32(U32(t + T_SP))) return 1;
    U32(t + T_PC) += 2;
    return 0;
}

u32 DC2CALL ScrOp_Obj44Init(gptr t)                                /* 44: 8 args -> G+0x948[i+1] */
{
    gptr sp = U32(t + T_SP);
    gptr e = U32(G() + 0x948) + (U32(sp + 0x1C) + 1) * 0x34;
    memset(MEM(e), 0, 0x34);
    U8(e + 0) = 1;
    U8(e + 1) = U8(sp + 0x18);
    U16(e + 0x0C) = U16(sp + 0x14);
    U16(e + 0x0E) = U16(sp + 0x10);
    U16(e + 0x10) = U16(sp + 0x0C);
    U16(e + 0x12) = U16(sp + 0x08);
    U16(e + 0x14) = U16(sp + 0x04);
    U16(e + 0x16) = U16(sp + 0x00);
    U32(t + T_PC) += 2;
    return 0;
}

u32 DC2CALL ScrOp_ObjEnable(gptr t)                                /* 45: obj_enable(kind, index, on) */
{
    gptr sp = U32(t + T_SP), g = G();
    u32 kind = U32(sp + 8), idx = U32(sp + 4), on = U32(sp) == 1;
    switch (kind) {
    case 0:
        if (!on) Obj_Disable(U32(g + 0x940) + idx * 0x260);
        break;
    case 1: {
        gptr f = U32(g + 0x93C) + idx * 0x70 + 0x590;
        U32(f) = on ? (U32(f) | 7) : (U32(f) & ~7u);
        break; }
    case 2: {
        gptr f = U32(g + 0x944) + idx * 0xAC + 0x50;
        U32(f) = on ? (U32(f) | 3) : (U32(f) & ~3u);
        break; }
    case 3: {
        gptr f = U32(g + 0x948) + (idx + 1) * 0x34;
        U8(f) = on ? (U8(f) | 1) : (U8(f) & 0xFE);
        break; }
    case 4: {
        gptr f = U32(g + 0xA5C) + idx * 0x28;              /* AOT */
        U8(f) = on ? (U8(f) | 1) : (U8(f) & 0xFE);
        break; }
    case 6: {
        gptr f = U32(g + 0xA44) + idx * 0x2C;              /* camera zone: bit 7 = off */
        U8(f) = on ? (U8(f) & 0x7F) : (U8(f) | 0x80);
        break; }
    case 8: {
        gptr c = U32(g + 0xA4C), f = c + U32(c + idx * 4 + 8) + 3;
        U8(f) = on ? (U8(f) & 3) : (U8(f) | 0xFC);         /* polygon ignored by all kinds */
        break; }
    default: break;
    }
    U32(t + T_PC) += 2;
    return 0;
}

u32 DC2CALL ScrOp_CutsceneCamera(gptr t)                           /* 4A: cam >= 0 starts, < 0 ends */
{
    s16 cam = S16(U32(t + T_SP));
    if (Flag_Check(2, 0)) return 1;
    if (cam >= 0) {
        if ((s16)U8(G() + 0x7C4) != cam) Camera_Switch((u32)(s32)cam);
        Flag_Set(1, 10, 1);
        Flag_Set(1, 1, 1);
        U16(0x87FC4A) = 0;
        U32(t + T_PC) += 2;
        return 0;
    }
    Flag_Set(1, 10, 0);
    Flag_Set(1, 1, 0);
    U16(0x87FC4A) = 0;
    U32(t + T_PC) += 2;
    return 1;
}
