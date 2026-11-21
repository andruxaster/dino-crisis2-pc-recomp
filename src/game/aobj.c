/* Area objects: the object types 17..28 of the area tables (*0x87DCA0)
 * other than the roamer (area 3 type 22, roamer.c). Placed by the room
 * script (obj_spawn), their parameters at o+0x1C0.. come from the script. */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

/* ---- type 17 of areas 1, 2 and 8: the falling piece (rooms 106, 202, 203) ----
 * It waits, shaking (its scale pulses), until event flag 2/0x19 (+0x1C0 bit
 * 0x80) or a timer (+0x1C4) (bit 0x10: it only shakes); then it moves along its
 * facing at speed +0x1C2, turns a quarter (bits 0x40/0x20, by its way of
 * hanging) after +0x1C6, and is gone after +0x1C8 more.
 *   +0x1C0 u16: low 4 bits how it hangs (1/2 pitched down/up, 3/4 its yaw
 *     made a roll), 0x100 drawn without o+0x50 bits 2/4, 0x200 sound 0x5C
 *     when it goes; +0x1CA/+0x1CC u16 turn added on the quarter turn;
 *   +0x1CE s16 the shake's scale step; +0x1D0 where it started or turned;
 *   +0x1D8 u16 the shake's frames, +0x1DA u8 shakes left, +0x1DB u8 its
 *   phase (1..3 smaller, 4..6 back), +0x1DC u8 frames of a phase.
 * States (o+0x54, 0x720284): 0 init, 1 normal by o+0x55 (0x720274: 0 wait,
 * 1 move, 2 move on), 2 gone. */
#define DR_MAIN 0x720284u
#define DR_MOVE 0x720274u

void DC2CALL Drop_Handler(gptr o)                                   /* 0x426C70 */
{
    if (U8(o + 0x1C1) & 1) U8(o + 0x50) &= 0xF9;
    else U8(o + 0x50) |= 6;
    U16(o + 0xE0) = U16(o + 0x40); U16(o + 0xE2) = U16(o + 0x42); U16(o + 0xE4) = U16(o + 0x44);
    Orig_Call1(U32(DR_MAIN + U8(o + 0x54) * 4), o);
    RotMatrix(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    RotMatrix(o + 0x48, o + 0x20);
    memcpy(MEM(U32(o + 0x64) + 0x20), MEM(o + 0x20), 0x20);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

/* the next shake: 30, 60 or 90 frames away, 1..4 of them */
static void drop_shakes(gptr o)
{
    U16(o + 0x1D8) = (u16)((Rand8() % 3 + 1) * 30);
    U8(o + 0x1DA) = (u8)((Rand8() & 3) + 1);
    U8(o + 0x1DB) = 0;
    U8(o + 0x1DC) = 5;
}

/* how it hangs: +0x1C0 low bits 1/2 pitched, 3/4 its yaw made the pitch, rolled */
static void drop_hang(gptr o)
{
    switch (U16(o + 0x1C0) & 0xF) {
    case 1: U16(o + 0x48) = 0xC00; break;
    case 2: U16(o + 0x48) = 0x400; break;
    case 3: U16(o + 0x48) = U16(o + 0x4A); U16(o + 0x4A) = 0; U16(o + 0x4C) = 0xC00; break;
    case 4: U16(o + 0x48) = U16(o + 0x4A); U16(o + 0x4A) = 0; U16(o + 0x4C) = 0x400; break;
    }
}

void DC2CALL Drop_Init(gptr o)                                      /* 0x426710 */
{
    U16(o + 0x108) = 0;
    U32(o + 0x50) = 0x20005;
    Model_Setup(o, U32(o + 0x60));
    drop_hang(o);
    U32(o + 0x1D0) = U32(o + 0x40); U32(o + 0x1D4) = U32(o + 0x44);
    drop_shakes(o);
    U8(o + 0x54)++;
}

void DC2CALL Drop_Normal(gptr o)                                    /* 0x426C40 */
{
    Orig_Call1(U32(DR_MOVE + U8(o + 0x55) * 4), o);
}

static void drop_scale(gptr o, s32 d)
{
    U32(o + 0x6C) += (u32)d; U32(o + 0x70) += (u32)d; U32(o + 0x74) += (u32)d;
}

/* step 0: shaking until it goes */
void DC2CALL Drop_Wait(gptr o)                                      /* 0x426800 */
{
    u16 f = U16(o + 0x1C0);
    int go = 0;
    if (!(f & 0x10)) {
        if (f & 0x80) go = Flag_Check(2, 0x19) != 0;
        else go = --S16(o + 0x1C4) <= 0;
    }
    if (go) {
        U16(o + 0xF4) = U16(o + 0x1C2);
        U8(o + 0x55)++;
        U32(o + 0x6C) = U32(o + 0x70) = U32(o + 0x74) = 0x1000;
        U32(G() + 0xB24) |= 0x80000000;
        if (U8(o + 0x1C1) & 2) Se_Play(o, 0x5C);
        return;
    }
    if (--U16(o + 0x1D8)) return;
    if (--U8(o + 0x1DC)) { U16(o + 0x1D8) = 1; return; }
    s32 d = S16(o + 0x1CE);
    u8 p = ++U8(o + 0x1DB);
    if (p > 3) {
        if (p <= 6) {
            U8(o + 0x1DC) = 5; U16(o + 0x1D8) = 1;
            drop_scale(o, d);
            return;
        }
        if (!--U8(o + 0x1DA)) { drop_shakes(o); return; }
        U8(o + 0x1DB) = 1;
    }
    U8(o + 0x1DC) = 5;
    drop_scale(o, -d);
    U16(o + 0x1D8) = 1;
}

/* the quarter turn (+0x1C0 bits 0x40/0x20, by how it hangs), then the turn
 * +0x1CA/+0x1CC added (also to the falling piece of type 19) */
static void drop_turn(gptr o)
{
    u16 f = U16(o + 0x1C0), m = f & 0xF;
    u32 x = U16(o + 0x48), z = U16(o + 0x4C);
    enum { NONE, A81, AA2, ADA, B07, B42, B68, B82, B9D } w = NONE;
    if (f & 0x40) {
        if (m >= 3) {
            s16 a = S16(o + 0x48);
            if (m == 3) w = a <= 0x400 ? A81 : a <= 0xC00 ? AA2 : A81;
            else w = a <= 0x400 ? ADA : a <= 0xC00 ? B07 : ADA;
        } else if (m == 1) w = U16(o + 0x4A) & 0x800 ? B68 : B42;
        else w = U16(o + 0x4A) & 0x800 ? B9D : B82;
    } else if (f & 0x20) {
        if (m >= 3) {
            s16 a = S16(o + 0x48);
            int in = a > 0x400 && a <= 0xC00;
            if (m == 3) w = in ? A81 : AA2;
            else w = in ? ADA : B07;
        } else if (m == 1) w = U16(o + 0x4A) & 0x800 ? B42 : B68;
        else w = U16(o + 0x4A) & 0x800 ? B82 : B9D;
    }
    u32 y = U16(o + 0x4A);
    switch (w) {                    /* the yaw (pitch for 3/4) and the roll swapped about */
    case A81: U16(o + 0x4A) = (u16)((x + 0x400) & 0xFFF); U16(o + 0x48) = (u16)z; U16(o + 0x4C) = 0; break;
    case AA2: U16(o + 0x4A) = (u16)((x - 0x400) & 0xFFF); U16(o + 0x48) = (u16)((z - 0x800) & 0xFFF); U16(o + 0x4C) = 0; break;
    case ADA: U16(o + 0x4A) = (u16)((-0x400 - x) & 0xFFF); U16(o + 0x48) = (u16)((z - 0x800) & 0xFFF); U16(o + 0x4C) = 0; break;
    case B07: U16(o + 0x48) = (u16)z; U16(o + 0x4A) = (u16)((0x400 - x) & 0xFFF); U16(o + 0x4C) = 0; break;
    case B42: U16(o + 0x4C) = (u16)((x - 0x800) & 0xFFF); U16(o + 0x48) = (u16)((-0x400 - y) & 0xFFF); U16(o + 0x4A) = 0; break;
    case B68: U16(o + 0x4C) = (u16)x; U16(o + 0x48) = (u16)((y - 0x400) & 0xFFF); U16(o + 0x4A) = 0; break;
    case B82: U16(o + 0x4C) = (u16)x; U16(o + 0x48) = (u16)((0x400 - y) & 0xFFF); U16(o + 0x4A) = 0; break;
    case B9D: U16(o + 0x4C) = (u16)((x - 0x800) & 0xFFF); U16(o + 0x48) = (u16)((y + 0x400) & 0xFFF); U16(o + 0x4A) = 0; break;
    case NONE: break;
    }
    U16(o + 0x48) = (u16)((U16(o + 0x1CA) + U16(o + 0x48)) & 0xFFF);
    U16(o + 0x4A) = (u16)((U16(o + 0x1CC) + U16(o + 0x4A)) & 0xFFF);
}

/* step 1: moving; at +0x1C6 from the start the quarter turn */
void DC2CALL Drop_Move(gptr o)                                      /* 0x4269A0 */
{
    if (Camera_Distance(o + 0x40, o + 0x1D0) <= (u32)(s32)S16(o + 0x1C6)) return;
    U8(o + 0x55)++;
    U32(o + 0x1D0) = U32(o + 0x40); U32(o + 0x1D4) = U32(o + 0x44);
    drop_turn(o);
}

/* step 2: moving on; at +0x1C8 from the turn it stops and is gone */
void DC2CALL Drop_MoveOn(gptr o)                                    /* 0x426C00 */
{
    if (Camera_Distance(o + 0x40, o + 0x1D0) <= (u32)(s32)S16(o + 0x1C8)) return;
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U8(o + 0x54)++;
}

/* ---- type 18 of areas 1, 2, 8 and 9: the flying shell (rooms 107, 200) ----
 * Flies straight at a point (+0x1C4) at speed +0x1CC, spinning; within 1000
 * of it it is gone, with a flash (+0x1C0 non-zero: sound 0x6A, a light of
 * radius +0x1CA and +0x1C2).
 * States (o+0x54, 0x720340): 0 init, 1 flying, 2 gone. */
#define SH_MAIN 0x720340u

void DC2CALL Shell_Handler(gptr o)                                  /* 0x426E50 */
{
    U16(o + 0xE0) = U16(o + 0x40); U16(o + 0xE2) = U16(o + 0x42); U16(o + 0xE4) = U16(o + 0x44);
    Orig_Call1(U32(SH_MAIN + U8(o + 0x54) * 4), o);
    if (U32(o + 0x50) & 0x40000000) { Obj_Disable(o); return; }
    U16(o + 0x4A) = (u16)(Angle_XZ(o + 0x40, o + 0x1C4) & 0xFFF);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Vec_Toward(o + 0x40, o + 0x1C4, U16(o + 0x1CC), o + 0xF0);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
}

void DC2CALL Shell_Init(gptr o)                                     /* 0x426D10 */
{
    U32(o + 0x134) = U32(o + 0x128) = U32(o + 0x12C) = 0x720290;
    U16(o + 0x108) = 0;
    U32(o + 0x50) = 3;
    Model_Setup(o, U32(o + 0x60));
    U16(o + 0xF4) = 0x190;
    U8(o + 0xF8) = U8(o + 0xF9) = U8(o + 0xFA) = 0xF0;
    Obj_Set118(o, 0, 0x1000, 0x1000);
    U8(o + 0x118) |= 0x20;
    U8(o + 0x54)++;
}

void DC2CALL Shell_Fly(gptr o)                                      /* 0x426DA0 */
{
    gptr p = U32(o + 0x64);
    if (!U16(o + 0x10C)) { U16(p + 0x4C) = 0x400; U16(p + 0x48) += 0xFEB0; }
    else U16(p + 0x4A) += 0x150;
    if (Dist2_XZ(o + 0x40, o + 0x1C4) > 1000000) return;
    if (U16(o + 0x1C0)) Shell_Flash(o);
    U8(o + 0x54) = 2;
}

void DC2CALL Shell_Flash(gptr o)                                    /* 0x426E00 */
{
    Se_Play(o, 0x6A);
    gptr l = Light_Alloc();
    if (!l) return;
    U8(l + 1) = 2;
    U32(l + 0xC) = 0x7202A0;
    U32(l + 0x14) = o;
    U16(l + 0x1E) = U16(o + 0x1C2);
    U32(l + 0x18) |= 4;
    U16(l + 0x1C) = U16(o + 0x1CA);
}

/* ---- type 19 of areas 2, 6, 7 and 9: the moving piece (rooms 603, 703, 704, 903) ----
 * Waits for event flag 2/0x19 (+0x1C0 bit 0x80) or a timer (+0x1C4; bit
 * 0x10: never), then by +0x1C0 low bits: below 5 like the falling piece
 * (moves at +0x1C2, the quarter turn after +0x1C6, gone after +0x1C8), from
 * 5 it flies to the point +0x1C6 and on to the point +0x1CC, steering at
 * them (0x3C, then 0xD0 a frame) and pitched at them, its speed falling by
 * +0x1E0 a frame and set again when spent (the low bytes of +0x1C2, +0x1D2;
 * their high bytes the fall).
 *   +0x1D4 where it started or turned, +0x1DC u32 the way left to the point.
 * States (o+0x54, 0x72036C): 0 init, 1 normal by o+0x55 (below 5 0x72034C:
 * wait, move, move on; from 5 0x72035C: wait, to the first point, to the
 * second), 2 gone. */
#define MV_MAIN 0x72036Cu
#define MV_DROP 0x72034Cu
#define MV_FLY  0x72035Cu
#define MV_FLIES(o) ((U8((o) + 0x1C0) & 0xF) >= 5)

void DC2CALL Mover_Handler(gptr o)                                  /* 0x4275B0 */
{
    if (U8(o + 0x1C1) & 1) U8(o + 0x50) &= 0xF9;
    else U8(o + 0x50) |= 6;
    U16(o + 0xE0) = U16(o + 0x44);                  /* (as the original: only this one) */
    Orig_Call1(U32(MV_MAIN + U8(o + 0x54) * 4), o);
    if (!MV_FLIES(o)) {
        RotMatrix(o + 0x48, o + 0x20);
        Obj_MoveLocal(o, o + 0xF0);
        Obj_UpdateMatrix(o);
        RotMatrix(o + 0x48, o + 0x20);
        memcpy(MEM(U32(o + 0x64) + 0x20), MEM(o + 0x20), 0x20);
    } else {
        RotMatrixYXZ(o + 0x48, o + 0x20);
        Obj_MoveLocal(o, o + 0xF0);
        Obj_UpdateMatrix(o);
        RotMatrixYXZ(o + 0x48, o + 0x20);
    }
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

void DC2CALL Mover_Init(gptr o)                                     /* 0x426EF0 */
{
    U16(o + 0x108) = 0;
    U32(o + 0x50) = 0x20005;
    Model_Setup(o, U32(o + 0x60));
    drop_hang(o);
    U32(o + 0x1D4) = U32(o + 0x40); U32(o + 0x1D8) = U32(o + 0x44);
    U8(o + 0x54)++;
}

void DC2CALL Mover_Normal(gptr o)                                   /* 0x427540 */
{
    if (!MV_FLIES(o)) Mover_Drop(o);
    else Mover_Fly(o);
}

void DC2CALL Mover_Drop(gptr o)                                     /* 0x427570 */
{
    Orig_Call1(U32(MV_DROP + U8(o + 0x55) * 4), o);
}

void DC2CALL Mover_Fly(gptr o)                                      /* 0x427590 */
{
    Orig_Call1(U32(MV_FLY + U8(o + 0x55) * 4), o);
}

/* step 0 of both */
void DC2CALL Mover_Wait(gptr o)                                     /* 0x426FA0 */
{
    u16 f = U16(o + 0x1C0);
    if (f & 0x10) return;
    if (f & 0x80) { if (!Flag_Check(2, 0x19)) return; }
    else if (--S16(o + 0x1C4) > 0) return;
    U32(o + 0x1DC) = Camera_Distance(o + 0x1D4, o + 0x1C6);
    U8(o + 0x55)++;
    u16 v = U16(o + 0x1C2);
    if (!MV_FLIES(o)) U16(o + 0xF4) = v;
    else { U16(o + 0xF4) = v & 0xFF; S16(o + 0x1E0) = (s16)((s16)v >> 8); }
    U32(G() + 0xB24) |= 0x80000000;
    if (U8(o + 0x1C1) & 2) Se_Play(o, 0x5C);
}

void DC2CALL Mover_Move(gptr o)                                     /* 0x427060 */
{
    if (Camera_Distance(o + 0x1D4, o + 0x40) < (u32)(s32)S16(o + 0x1C6)) return;
    U8(o + 0x55)++;
    U32(o + 0x1D4) = U32(o + 0x40); U32(o + 0x1D8) = U32(o + 0x44);
    drop_turn(o);
}

void DC2CALL Mover_MoveOn(gptr o)                                   /* 0x4272C0 */
{
    if (Camera_Distance(o + 0x40, o + 0x1D4) < (u32)(s32)S16(o + 0x1C8)) return;
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U8(o + 0x54)++;
}

/* the pitch from a at b, seen turned by yaw */
u32 DC2CALL Mover_Pitch(gptr a, gptr b, u32 yaw)                    /* 0x4273E0 */
{
    gptr f = Tmp_Push(0x28), r = f, m = f + 8;
    U16(r) = 0; U16(r + 2) = (u16)(-yaw & 0xFFF); U16(r + 4) = 0;
    RotMatrix(r, m);
    U16(r) = (u16)(U16(b) - U16(a)); U16(r + 2) = (u16)(U16(b + 2) - U16(a + 2)); U16(r + 4) = (u16)(U16(b + 4) - U16(a + 4));
    ApplyMatrixSV(m, r, r);
    s32 y = S16(r + 2), z = S16(r + 4);
    Tmp_Pop(0x28);
    return -(ratan2((float)y, (float)z) & 0xFFF) & 0xFFF;
}

/* steer and pitch at point p by at most `turn`; the speed falls, or is set
   again to `top` once spent */
static void mover_steer(gptr o, gptr p, u32 turn, u32 top)
{
    u32 a = Angle_XZ(o + 0x40, p);
    U16(o + 0x4A) = (u16)((Angle_Approach(U16(o + 0x4A), turn, a) + U16(o + 0x4A)) & 0xFFF);
    U16(o + 0x48) = (u16)(Mover_Pitch(o + 0x40, p, U16(o + 0x4A)) & 0xFFF);
    if (S16(o + 0xF4) > 0) U16(o + 0xF4) -= U16(o + 0x1E0);
    else U16(o + 0xF4) = (u16)top;
}

/* step 1 (flying): to the first point; there on to the second */
void DC2CALL Mover_ToFirst(gptr o)                                  /* 0x427300 */
{
    if (Camera_Distance(o + 0x40, o + 0x1D4) < U32(o + 0x1DC)) { mover_steer(o, o + 0x1C6, 0x3C, U8(o + 0x1C2)); return; }
    U32(o + 0x1DC) = Camera_Distance(o + 0x40, o + 0x1CC);
    u16 v = U16(o + 0x1D2);
    U32(o + 0x1D4) = U32(o + 0x40); U32(o + 0x1D8) = U32(o + 0x44);
    S16(o + 0x1E0) = (s16)((s16)v >> 8);
    U16(o + 0xF4) = v & 0xFF;
    U8(o + 0x55)++;
}

/* step 2 (flying): to the second point; there it stops, gone */
void DC2CALL Mover_ToSecond(gptr o)                                 /* 0x427490 */
{
    if (Camera_Distance(o + 0x40, o + 0x1D4) < U32(o + 0x1DC)) { mover_steer(o, o + 0x1CC, 0xD0, U8(o + 0x1D2)); return; }
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U8(o + 0x54)++;
}

/* ---- type 21 of area 1: the bird (rooms 10A, 10B, 10C) ----
 * Bobs where it sits (up and down 10, now and then a hop by 0x720378) until
 * the player fires or runs (or, +0x1C0 bit 1, until +0x1D0 is set), then
 * flies off to the point +0x1C4 and on to the point +0x1CA at speed +0x1C2,
 * steering and pitching (+0x1D2) at them by at most 0x3C a frame.
 *   +0x1D4 where it set off from, +0x1DC u32 the way to the next point;
 *   +0x1E0 u8 the hop's step, +0x1E1 u8 frames to the next one, +0x1E2 u8
 *   up or down.
 * States (o+0x54, 0x7203B4): 0 init, 1 normal: animation, then by o+0x55
 * (0x7203A4: 0 sits, 1 to the first point, 2 to the second), 2 gone. */
#define BD_MAIN 0x7203B4u
#define BD_MOVE 0x7203A4u
#define BD_HOP  0x720378u

void DC2CALL Bird_Handler(gptr o)                                   /* 0x427A50 */
{
    U16(o + 0xE0) = U16(o + 0x40); U16(o + 0xE2) = U16(o + 0x42); U16(o + 0xE4) = U16(o + 0x44);
    Orig_Call1(U32(BD_MAIN + U8(o + 0x54) * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

void DC2CALL Bird_Init(gptr o)                                      /* 0x427670 */
{
    U16(o + 0x108) = 0;
    U32(o + 0x50) = 0x20005;
    Model_Setup(o, U32(o + 0x60));
    U8(o + 0x1E0) = 0;
    U8(o + 0x1E1) = (u8)((Rand8() % 9 + 5) << 2);
    U8(o + 0x1E2) = 0;
    U8(o + 0x54)++;
}

void DC2CALL Bird_Normal(gptr o)                                    /* 0x427A30 */
{
    Anim_Step(o);
    Orig_Call1(U32(BD_MOVE + U8(o + 0x55) * 4), o);
}

/* step 0: sits bobbing until the player fires or runs */
void DC2CALL Bird_Sit(gptr o)                                       /* 0x4276D0 */
{
    int go;
    if (U8(o + 0x1C0) & 1) go = U16(o + 0x1D0) != 0;
    else {
        gptr p = U32(G() + 0x940);
        go = U16(p + 0x54) == 0x201
          || (U8(p + 0x54) == 1 && ((U8(p + 0x55) == 5 && (U8(p + 0x56) == 5 || U8(p + 0x56) == 6)) || U8(p + 0x55) == 0xB));
    }
    if (go) {
        U32(o + 0x1D4) = U32(o + 0x40); U32(o + 0x1D8) = U32(o + 0x44);
        U32(o + 0x1DC) = Camera_Distance(o + 0x1D4, o + 0x1C4);
        U16(o + 0x1D2) = U16(o + 0x48);
        U8(o + 0x55)++;
        return;
    }
    U16(o + 0x42) += U8(o + 0x1E2) ? 10 : (u16)-10;
    U8(o + 0x1E2) ^= 1;
    if (--U8(o + 0x1E1)) return;
    u8 i = U8(o + 0x1E0);
    U16(o + 0x42) += U16(BD_HOP + i * 2);
    U8(o + 0x1E0) = ++i;
    if (i < 0x16) { U8(o + 0x1E1) = 1; return; }
    U8(o + 0x1E0) = 0;
    U8(o + 0x1E1) = (u8)((Rand8() % 9 + 5) << 2);
}

/* steer and pitch at point p, then a step of +0x1C2 along the way it faces */
static void bird_steer(gptr o, gptr p)
{
    u32 a = Angle_XZ(o + 0x40, p);
    U16(o + 0x4A) = (u16)((Angle_Approach(U16(o + 0x4A), 0x3C, a) + U16(o + 0x4A)) & 0xFFF);
    u32 t = Mover_Pitch(o + 0x40, p, U16(o + 0x4A));
    U16(o + 0x1D2) = (u16)((Angle_Approach(U16(o + 0x1D2), 0x3C, t) + U16(o + 0x1D2)) & 0xFFF);
    gptr f = Tmp_Push(0x28), v = f, m = f + 8;
    U16(v) = U16(o + 0x1D2); U16(v + 2) = U16(o + 0x4A); U16(v + 4) = 0;
    RotMatrixYXZ(v, m);
    U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = U16(o + 0x1C2);
    ApplyMatrixSV(m, v, v);
    U16(o + 0x40) += U16(v); U16(o + 0x42) += U16(v + 2); U16(o + 0x44) += U16(v + 4);
    Tmp_Pop(0x28);
}

/* step 1: to the first point; there on to the second */
void DC2CALL Bird_ToFirst(gptr o)                                   /* 0x427800 */
{
    u32 d = Camera_Distance(o + 0x1D4, o + 0x40);
    U32(0x74AF0C) = d;
    if (d < U32(o + 0x1DC)) { bird_steer(o, o + 0x1C4); return; }
    U32(o + 0x1D4) = U32(o + 0x40); U32(o + 0x1D8) = U32(o + 0x44);
    U32(o + 0x1DC) = Camera_Distance(o + 0x1D4, o + 0x1CA);
    U8(o + 0x55)++;
}

/* step 2: to the second point; there it stops, gone */
void DC2CALL Bird_ToSecond(gptr o)                                  /* 0x427920 */
{
    u32 d = Camera_Distance(o + 0x1D4, o + 0x40);
    U32(0x74AF08) = d;
    if (d < U32(o + 0x1DC)) { bird_steer(o, o + 0x1CA); return; }
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U8(o + 0x54)++;
}

/* ---- type 23 of area 8: the rock that breaks (rooms 805, 806, 807) ----
 * Waits to be hit (o+0x174); hit by weapon 0x13 or 0x15 it is gone, by
 * anything else it shakes the hit off (o+0x17B 0x14). With o+0x10C set it
 * is not hit by the player's bullets (o+0x50 bit 8 off) and never breaks.
 * States (o+0x54, 0x7203CC): 0 init, 1 waits, 2 hit, 3 Obj_Disable. */
#define RK_MAIN 0x7203CCu

void DC2CALL Rock_Handler(gptr o)                                   /* 0x427AD0 */
{
    Obj_SavePos(o);
    Orig_Call1(U32(RK_MAIN + U8(o + 0x54) * 4), o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    RotMatrix(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

void DC2CALL Rock_Init(gptr o)                                      /* 0x427B40 */
{
    gptr p = U32(G() + 0x940);
    U16(o + 0x108) = 0;
    U32(o + 0x50) = 0x3001F;
    U32(o + 0x134) = U32(o + 0x128) = 0x7203C0;
    Model_Setup(o, U32(o + 0x60));
    U16(o + 0xF0) = 0; U16(o + 0xF2) = 0; U16(o + 0xF4) = 0;
    U32(o + 0x6C) = U32(o + 0x70) = U32(o + 0x74) = 0x1000;
    U8(o + 0xF8) = U8(o + 0xF9) = U8(o + 0xFA) = 0xF0;
    U8(o + 0x1C0) = 0;
    U16(o + 0x1C6) = U16(p + 0x40); U16(o + 0x1C8) = U16(p + 0x42); U16(o + 0x1CA) = U16(p + 0x44);
    U16(o + 0x1C4) = 0xFED4;
    if (U16(o + 0x10C)) U32(o + 0x50) &= ~8u;
    U8(o + 0x54)++;
}

void DC2CALL Rock_Wait(gptr o)                                      /* 0x427C00 */
{
    if (!U16(o + 0x174)) return;
    U16(o + 0x174) = 0;
    U8(o + 0x54) = 2;
}

void DC2CALL Rock_Hit(gptr o)                                       /* 0x427C20 */
{
    if (U16(o + 0x10C)) return;
    u8 w = U8(o + 0x17D);
    if (w == 0x13 || w == 0x15) { U32(o + 0x50) |= 0x40000000; return; }
    U8(o + 0x17B) = 0x14;
    U8(o + 0x54) = 1;
}

/* turn from cur toward the way from a to b: the difference, or at most max
 * (when it is twice max or more), negative to the left */
u32 DC2CALL Angle_TurnMax(gptr a, gptr b, u32 cur, u32 max)         /* 0x44B430 */
{
    s32 d = (s32)((Angle_XZ(a, b) - cur) & 0xFFF), m2 = (s32)(s16)max * 2;
    u32 r = max;
    if (d < 0x800) { if (d < m2) r = (u32)d; }
    else if (0x1000 - d < m2) r = (u32)(0x1000 - d);
    return d > 0x800 ? (u32)-(s32)r : r;
}

/* ---- type 27 of every area: the insect (type 26 in area 6) ----
 * Flies about the player bobbing up and down, buzzing (sound 0xA/0xB by
 * turns); near the player for a while it lands on it (the player knocked
 * into state 2 unless busy) and flies off again; gone when hit (it falls
 * apart: parts 0..5 thrown out, 0x720430), 150 frames off screen, or when
 * the camera changes after the player let it go (o+0x17E of the player 0).
 *   o+0x10C its kind (1 on easy G+0x108E 0, else 2; 0x720414: speed, turn);
 *   +0x1C0 u8 frames off screen, +0x1C1 u8 the bob's phase, +0x1C3 u8 its
 *   step, +0x1C4 u8 the buzz's timer, +0x1C5 u8 which buzz, +0x1C6 u8 the
 *   camera, +0x1C8 u16 frames in the state, +0x1CA u16, +0x1CC u16 frames to
 *   fall apart, +0x1D0 u16 the way from the player, +0x1D8 the player's
 *   place when it left, +0x1E4 u8[6] the parts' scatter.
 * States (o+0x54, 0x72041C): 0 init, 1 flying about, 2 on the player, 3
 * leaving, 4 hit. */
#define BG_MAIN 0x72041Cu
#define BG_KIND 0x720414u
#define BG_FALL 0x720430u

void DC2CALL Bug_Handler(gptr o)                                    /* 0x428230 */
{
    U16(o + 0xE0) = U16(o + 0x40); U16(o + 0xE2) = U16(o + 0x42); U16(o + 0xE4) = U16(o + 0x44);
    if (U16(o + 0x174) & 1) {
        U16(o + 0x174) &= 0xFFFE;
        U8(o + 0x54) = 4;
        U8(o + 0x50) &= 0xF7;
        U8(o + 0x55) = 0;
    }
    Orig_Call1(U32(BG_MAIN + U8(o + 0x54) * 4), o);
    U16(o + 0x1C8)++;
    U8(o + 0x1C6) = U8(G() + 0x7C4);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    if (U8(o + 0x50) & 2) U8(o + 0x1C0) = 0;
    else if (++U8(o + 0x1C0) > 0x96) U32(o + 0x50) |= 0x40000000;
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

void DC2CALL Bug_Init(gptr o)                                       /* 0x428310 */
{
    U16(o + 0x10C) = U8(G() + 0x108E) == 0 ? 1 : 2;
    U16(o + 0x108) = 0;
    U32(o + 0x50) = 0x3000F;
    U32(o + 0x134) = 0x720408;
    Model_Setup(o, 0x681500);
    U16(o + 0xF4) = U8(BG_KIND + U16(o + 0x10C) * 2);
    U16(o + 0x1CA) = 0xC8;
    U32(o + 0x184) |= 2;
    U8(o + 0x1C0) = 0;
    U8(o + 0x1C1) = Rand8();
    U8(o + 0x1C3) = 0x10;
    U8(o + 0x1C5) = 0;
    U8(o + 0x1C4) = (u8)(Rand8() % 0x5A);
    U16(o + 0x1C8) = 0;
    Anim_Start(o, 0x6819F4, 0, 1, 0);
    U8(o + 0x54) = 1;
}

/* the buzz every n frames, by turns */
static void bug_buzz(gptr o, u32 n)
{
    u8 i = ++U8(o + 0x1C4);
    if (i % n) return;
    Se_Play(o, U8(o + 0x1C5) + 10);
    U8(o + 0x1C5) = (u8)(1 - U8(o + 0x1C5));
}

/* state 1: about the player */
void DC2CALL Bug_Chase(gptr o)                                      /* 0x4283F0 */
{
    gptr p = U32(G() + 0x940);
    Anim_Step(o);
    bug_buzz(o, 30);
    Bug_Fly(o, p + 0x40);
    u32 d = Dist2_XZ(o + 0x40, p + 0x40);
    u16 k = U16(o + 0x10C);
    if (k >= 2) U16(o + 0xF4) = (u16)(U8(BG_KIND + k * 2) + (d > 4000000 ? 0x30 : 0));
    if (d < 0x57E4 && U16(o + 0x1C8) > 0x3C) { U8(o + 0x54) = 2; U16(o + 0x1C8) = 0; }
    if (U16(p + 0x17E)) return;
    U8(o + 0x54) = 3;
    U16(o + 0xF4) = (u16)((Rand8() & 0x1F) + 0x20);
    U16(o + 0x10C) = Rand8() & 1;
    U32(o + 0x1D8) = U32(p + 0x40); U32(o + 0x1DC) = U32(p + 0x44);
}

/* state 2: on the player (out of sight); after 0x1E frames, or once the
   player is busy, off again from above it */
void DC2CALL Bug_Land(gptr o)                                       /* 0x428520 */
{
    gptr p = U32(G() + 0x940);
    u16 c = U16(o + 0x1C8);
    if (c == 1) { U32(o + 0x50) &= ~0xEu; Se_StopGroup(0xA); return; }
    if (c <= 0x1E && U8(p + 0x54) == 1) return;
    if (U8(p + 0x54) == 1 && U8(p + 0x55) <= 7) {
        U16(p + 0x178) = 0; U16(p + 0x176) = 0;
        U8(p + 0x54) = 2; U8(p + 0x55) = 0; U8(p + 0x56) = 0;
    }
    U16(o + 0x40) = U16(p + 0x40); U16(o + 0x42) = (u16)(U16(p + 0x42) - 0x578); U16(o + 0x44) = U16(p + 0x44);
    U16(o + 0x4A) = Rand8();
    U8(o + 0x54) = 1;
    U32(o + 0x50) |= 0xE;
    U16(o + 0x1C8) = 0;
}

/* state 3: to where the player was; gone when the camera changes */
void DC2CALL Bug_Leave(gptr o)                                      /* 0x4285D0 */
{
    Anim_Step(o);
    bug_buzz(o, 0x5A);
    Bug_Fly(o, o + 0x1D8);
    if (U8(o + 0x1C6) != U8(G() + 0x7C4)) U32(o + 0x50) |= 0x40000000;
}

/* state 4: hit; falls apart in five frames */
void DC2CALL Bug_Die(gptr o)                                        /* 0x428650 */
{
    gptr p = U32(G() + 0x940);
    switch (U8(o + 0x55)) {
    case 0:
        U8(o + 0x55)++;
        U16(o + 0x1D0) = (u16)Angle_XZ(o + 0x40, p + 0x40);
        U16(o + 0x1CC) = 5; U16(o + 0x1CA) = 1;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        for (u32 i = 0; i < 6; i++) {
            U32(Obj_Part(U32(o + 0x64), i) + 0x58) = o;
            U8(o + 0x1E4 + i) = Rand8();
        }
        break;
    case 1:
        if (!U16(o + 0x1CC)) { U32(o + 0x50) |= 0x40000000; break; }
        U16(o + 0x1CC)--;
        for (u32 i = 0; i < 6; i++) {
            gptr t = BG_FALL + i * 4, q = Obj_Part(U32(o + 0x64), i);
            s32 a = (s16)(((s16)S8(o + 0x1E4 + i) + U16(t + 2) + U16(o + 0x1D0)) & 0xFFF);
            s32 dv = S8(t);
            U16(q + 0x64) += (u16)(rsin(a) / dv);
            U16(q + 0x68) += (u16)(rcos(a) / dv);
            U16(q + 0x66) += (u16)(s16)S8(t + 1);
            U16(q + 0x48) += 0x28;
        }
        U16(o + 0x1CA) += 2;
        break;
    }
}

/* the flight at a point: bobbing (+0x1C1 by +0x1C3, now and then a new
   step), kept 100..2000 above it, turning at it (0x720415 by kind) */
void DC2CALL Bug_Fly(gptr o, gptr t)                                /* 0x428790 */
{
    U8(o + 0x1C1) += U8(o + 0x1C3);
    U16(o + 0x42) += (u16)(rsin((u32)U8(o + 0x1C1) << 4) >> 7);
    s32 y = S16(o + 0x42), c = S16(t + 2);
    if (y > c - 0x64) U16(o + 0x42) -= 10;
    else if (y < c - 0x7D0) U16(o + 0x42) += 10;
    if (!(Rand8() & 0xF)) U8(o + 0x1C3) = (u8)((Rand8() & 7) + 4);
    U16(o + 0x4A) = (u16)((Angle_TurnMax(o + 0x40, t, U16(o + 0x4A), U8(BG_KIND + 1 + U16(o + 0x10C) * 2)) + U16(o + 0x4A)) & 0xFFF);
}

/* ---- type 22 of area 1: the poison plant (rooms 10D, 10E) ----
 * Lies hidden; the player stepping into its quad (+0x1F4: four x, z) or now
 * and then by chance, it rises (sound 0x5E) and snaps at a quad before it
 * (+0x1D4: four points, stretched out frame by frame): caught, the player
 * goes into state 2 and loses 2/5 of its full health. Kind +0x10C non-zero
 * (the grown one) never rises: a cloud (0x720448) every 16 frames, its quad
 * poisons for 3/5 (o+0x174 of the player set). Killed (o+0x170 bit 2) for
 * good (event flag 6/+0x1C0), it wilts (its parts' colours fade).
 *   +0x1C2 u16 frames of the snap, +0x1C6 u8 the cloud's timer, +0x1C8/
 *   +0x1CC the scale frames (u16[3] a part a frame: alive, wilting), +0x1D0
 *   the ones in use, +0x205 u8 its step before the hit, +0x206 u8 which
 *   sound, +0x207 s8 frames to it, +0x208 s8 sounds played.
 * States (o+0x54, 0x7206E0): 0 init, 1 set up, 2 hit, 3 gone, 4 normal by
 * o+0x55 (0x7206F4: 0 hidden, 1 rises, 2 snaps, 3 sinks back). */
#define PL_MAIN 0x7206E0u
#define PL_MOVE 0x7206F4u

void DC2CALL Plant_Handler(gptr o)                                  /* 0x428840 */
{
    Orig_Call1(U32(PL_MAIN + U8(o + 0x54) * 4), o);
    U8(o + 0x205) = U8(o + 0x55);
    RotMatrix(o + 0x48, o + 0x20);
    Plant_Matrix(o);
    u32 e = U32(o + 0x50);
    if ((e & 2) && U8(o + 0x54) == 4) e |= 8;
    else e &= ~8u;
    U32(o + 0x50) = e;
    if (e & 0x40000000) Obj_Disable(o);
}

void DC2CALL Plant_Init(gptr o)                                     /* 0x4288A0 */
{
    U32(o + 0x50) = 1;
    U8(o + 0x54)++;
    Model_Setup(o, U32(o + 0x60));
    Flag_Set(2, 0x14, 1);
}

/* state 1: gone for good if killed before; its spheres, the quad where it
   catches the player (wider for the grown one) */
void DC2CALL Plant_Setup(gptr o)                                    /* 0x4288D0 */
{
    gptr p = U32(G() + 0x940);
    U8(o + 0x55) = 0; U8(o + 0x56) = 0;
    U8(o + 0x54) += 3;
    if (Flag_Check(6, (u32)(s32)S16(o + 0x1C0))) { U8(o + 0x54)--; return; }
    U16(o + 0x108) = 0;
    U32(p + 0x50) |= 0x80;
    U32(o + 0x50) = 0x1D;
    U32(o + 0x12C) = 0x7206C8;
    U32(o + 0x134) = 0x7206D4;
    Plant_SetFrames(o, U32(o + 0x1C8));
    Anim_Start(o, 0, 0, 0, 0);
    s16 fz = (s16)(U16(o + 0x10C) ? -0x11F8 : -0x8FC);
    static const s16 qx[4] = {0x12C, 0x320, -0x320, -0x12C};
    gptr q = Tmp_Push(0x20);
    for (u32 i = 0; i < 4; i++) {
        S16(q + i * 8) = qx[i]; U16(q + i * 8 + 2) = 0;     /* (y of the third: not set in the original) */
        S16(q + i * 8 + 4) = (s16)(i == 1 || i == 2 ? fz : 0);
    }
    for (u32 i = 0; i < 4; i++) {
        gptr v = q + i * 8;
        ApplyMatrixSV(o + 0x20, v, v);
        U16(o + 0x1F4 + i * 4) = (u16)(U16(v) + U16(o + 0x40));
        U16(o + 0x1F6 + i * 4) = (u16)(U16(v + 4) + U16(o + 0x44));
    }
    Tmp_Pop(0x20);
}

/* the player caught: state 2, frac fifths of its full health off */
static void plant_catch(gptr p, s32 frac)
{
    U8(p + 0x54) = 2; U8(p + 0x55) = 0; U8(p + 0x56) = 0;
    U16(p + 0x108) += (u16)(-frac * S16(p + 0x10A) / 5);
}

/* step 0: hidden */
void DC2CALL Plant_Hidden(gptr o)                                   /* 0x428A10 */
{
    gptr p = U32(G() + 0x940);
    u32 in = PointInQuad(p + 0x40, o + 0x1F4);
    if (U16(o + 0x10C)) {
        u8 c = U8(o + 0x1C6)++;
        if (c > 0xF) {
            U8(o + 0x1C6) = 0;
            Effect_Spawn(o, 0x720448, 0, 0, 0x81, 0);
            Se_Play(o, 0x5E);
        }
        if (in && !(U8(p + 0x174) & 1) && U16(G() + 0x664) != 4) {
            plant_catch(p, 3);
            U16(p + 0x174) = 1;
        }
    } else if (Rand8() == 0x7A || in) {
        U8(o + 0x55)++;
        Anim_Start(o, 0, 0, 0, 0);
        Plant_SetFrames(o, U32(o + 0x1C8));
        Effect_Spawn(o, 0x720448, 0, 0, 0x80, 0);
    }
    if (S16(p + 0x108) < 0) GameOver_Start(0);
}

/* step 1: rising; at frame 0x16 the snap's quad, sound 0x5E */
void DC2CALL Plant_Rise(gptr o)                                     /* 0x428B60 */
{
    Anim_Step(o);
    if (U16(o + 0x92) <= 0x15) return;
    U16(o + 0x1D4) = 0x12C; U16(o + 0x1D6) = 0; U16(o + 0x1D8) = 0;
    U16(o + 0x1DC) = 0x12C; U16(o + 0x1DE) = 0; U16(o + 0x1E0) = 0;
    U16(o + 0x1E4) = 0xFED4; U16(o + 0x1E8) = 0;
    U16(o + 0x1EC) = 0xFED4; U16(o + 0x1EE) = 0; U16(o + 0x1F0) = 0;
    U16(o + 0x1C2) = 0;
    U8(o + 0x55)++;
    Se_Play(o, 0x5E);
}

/* step 2: the snap, its far side out 0x32 sideways and 0xE6 ahead a frame,
   10 frames */
void DC2CALL Plant_Snap(gptr o)                                     /* 0x428BF0 */
{
    gptr p = U32(G() + 0x940);
    U16(o + 0x1DC) += 0x32; U16(o + 0x1E0) += 0xFF1A;
    U16(o + 0x1E4) += 0xFFCE; U16(o + 0x1E8) += 0xFF1A;
    gptr t = Tmp_Push(0x30), q = t + 0x20;
    for (u32 i = 0; i < 4; i++) {
        ApplyMatrixSV(o + 0x20, o + 0x1D4 + i * 8, t + i * 8);
        U16(q + i * 4) = (u16)(U16(o + 0x40) + U16(t + i * 8));
        U16(q + i * 4 + 2) = (u16)(U16(o + 0x44) + U16(t + i * 8 + 4));
    }
    u32 in = PointInQuad(p + 0x40, q);
    Tmp_Pop(0x30);
    if (in && U16(G() + 0x664) != 4) {
        plant_catch(p, 2);
        U8(o + 0x55)++;
        if (S16(p + 0x108) < 0) { GameOver_Start(0); Anim_Step(o); return; }
    } else if (++S16(o + 0x1C2) > 10) U8(o + 0x55)++;
    Anim_Step(o);
}

/* step 3: sinks back; hidden at the animation's end */
void DC2CALL Plant_Sink(gptr o)                                     /* 0x428D70 */
{
    U16(o + 0x102) &= 0xFFDF;
    if (Anim_Step(o)) U8(o + 0x55) = 0;
}

void DC2CALL Plant_Normal(gptr o)                                   /* 0x428DA0 */
{
    Orig_Call1(U32(PL_MOVE + U8(o + 0x55) * 4), o);
}

/* state 2: hit; killed, it wilts (sounds 0x5F/0x60 by turns, its parts'
   colours down a step a frame), then gone; else back to its step */
void DC2CALL Plant_Hit(gptr o)                                      /* 0x428DC0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        if (!(U8(o + 0x170) & 2)) {
            U8(o + 0x54) += 2;
            U8(o + 0x55) = U8(o + 0x205);
            Plant_Normal(o);
            return;
        }
        Flag_Set(6, (u32)(s32)S16(o + 0x1C0), 1);
        U8(o + 0x56)++;
        U32(o + 0x50) &= ~8u;
        Effect_Spawn(o, 0x720510, 0, 0, 0x80, 0);
        U16(o + 0x1C2) = 0;
        Anim_Start(o, 1, 0, 8, 0x14);
        Plant_SetFrames(o, U32(o + 0x1CC));
        Score_Kill(o);
        /* fall through */
    case 1: {
        if (--S8(o + 0x207) < 0 && S8(o + 0x208) < 7) {
            U8(o + 0x207) = 0x28;
            U8(o + 0x206) ^= 1;
            U8(o + 0x208)++;
            Se_Play(o, U8(o + 0x206) + 0x5F);
        }
        gptr q = U32(o + 0x64);
        if (U8(q + 0x8C))
            for (u32 i = 0; i < U32(o + 0x68); i++) {
                U8(q + 0x8C)--; U8(q + 0x8D)--; U8(q + 0x8E)--;
                q = U32(q + 0x54);
            }
        break; }
    }
    if (Anim_Step(o)) U8(o + 0x54)++;
}

void DC2CALL Plant_SetFrames(gptr o, gptr f)                        /* 0x428F20 */
{
    U32(o + 0x1D0) = f;
}

/* its matrix scaled by o+0x6C, each part's scaled by the frame's u16[3] */
void DC2CALL Plant_Matrix(gptr o)                                   /* 0x428F30 */
{
    gptr f = U32(o + 0x1D0) + (u32)((s32)S16(o + 0x94) * (s32)U32(o + 0x68) * 6);
    if (!f) return;
    RotMatrix(o + 0x48, o + 0x20);
    ScaleMatrix(o + 0x20, o + 0x6C);
    U32(o + 0x34) = (u32)(s32)S16(o + 0x40); U32(o + 0x38) = (u32)(s32)S16(o + 0x42); U32(o + 0x3C) = (u32)(s32)S16(o + 0x44);
    gptr v = Tmp_Push(0x10);
    for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54)) {
        gptr par = U32(p + 0x58);
        RotMatrixYXZ(p + 0x48, p + 0x20);
        U32(v) = (u32)(s32)S16(f); U32(v + 4) = (u32)(s32)S16(f + 2); U32(v + 8) = (u32)(s32)S16(f + 4);
        f += 6;
        ScaleMatrix(p + 0x20, v);
        ApplyMatrix(par + 0x20, p + 0x64, p + 0x34);
        U16(p + 0x44) = U16(p + 0x3C); U16(p + 0x40) = U16(p + 0x34); U16(p + 0x42) = U16(p + 0x38);
        MulMatrix0(par + 0x20, p + 0x20, p + 0x20);
        U16(p + 0x40) += U16(par + 0x40); U16(p + 0x42) += U16(par + 0x42); U16(p + 0x44) += U16(par + 0x44);
    }
    Tmp_Pop(0x10);
}

/* a light's step by its script (0x71E1B4 by l+3); the next step when the
   script's record is the end (-1) */
void DC2CALL Light_Step(gptr l)                                     /* 0x413270 */
{
    Orig_Call1(U32(0x71E1B4 + (s32)S8(l + 3) * 4), l);
    if (U32(U32(l + 0xC)) == 0xFFFFFFFF) U8(l + 3)++;
}

/* ---- type 26 of area 4: the burner (room 402, the missile silo) ----
 * Off, then (+0x1C0 set) after 3 frames it burns for 150 frames (sound
 * 0x6A, its lamp, effect slot +0x1C6, turned red; flame lights 0x5ED530 /
 * 0x5ED924 by +0x1C4, the flame burning what is ahead: Weapon_Hit 0x19),
 * cools for 300 and is off again. G+0xB54 bit 0x40000000 (set by the room)
 * shows it: the camera 0x27/0x28 (+0x1C6 5) for 60 frames, event flags
 * 1/0, 1/1, 1/9, a light; then state 2, a longer burn (sound 0x6B, its hit
 * sets G+0xB54 bit 0x80000000).
 *   +0x1C2 u16 frames left, +0x1C8 u16 frames of the camera, +0x1CA u16
 *   sound 0x6B on, +0x1CC u16 the start's delay, +0x1CE u8 the camera
 *   before.
 * States (o+0x54, 0x7203F8): 0 init, 1 normal by o+0x55 (0x7203EC: 0 off,
 * 1 burning, 2 cooling), 2 the long burn, 3 nothing. */
#define BN_MAIN 0x7203F8u
#define BN_MOVE 0x7203ECu
#define BN_SLOT(o) (U32(0x87E764) + (u32)U16((o) + 0x1C6) * 0xAC)

void DC2CALL Burner_Handler(gptr o)                                 /* 0x428080 */
{
    gptr g = G();
    if ((U8(o + 0x54) != 1 || U8(o + 0x56)) && (U32(g + 0xB54) & 0x40000000) && !U16(o + 0x1C8) && !Flag_Check(4, 0xC)) {
        Flag_Set(1, 1, 1);
        u8 c = U8(G() + 0x7C4);
        u32 want = (U16(o + 0x1C6) != 5) + 0x27;
        if (c != want) {
            U8(o + 0x1CE) = c;
            U8(G() + 0x7C4) = (u8)want;
            Camera_Switch(want);
            gptr l = Light_Alloc();
            if (l) {
                U8(l + 1) = 1; U8(l + 2) = 1;
                U32(l + 0xC) = 0x5EC174;
                U8(l + 0x14) = 2;
                U32(l + 0x10) = U32(G() + 0xB54) & 0x20000000 ? 0x5EE4C0 : 0x5EDC74;
                EffSlots(l);
            }
        }
        U16(o + 0x1C8) = 0x3C;
        Flag_Set(1, 0, 1); Flag_Set(1, 9, 1);
        U32(G() + 0xB54) &= ~0x40000000u;
    }
    if (U16(o + 0x1C8) && --S16(o + 0x1C8) <= 0) {
        Flag_Set(1, 1, 0); Flag_Set(1, 0, 0); Flag_Set(1, 9, 0);
        U8(G() + 0x7C4) = U8(o + 0x1CE);
        Camera_Switch(U8(o + 0x1CE));
        U16(o + 0x1C8) = 0;
    }
    Orig_Call1(U32(BN_MAIN + U8(o + 0x54) * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
}

void DC2CALL Burner_Init(gptr o)                                    /* 0x427C60 */
{
    U32(o + 0x50) = 0x11;
    Model_Setup(o, U32(o + 0x60));
    U8(o + 0x54)++;
    U32(o + 0x134) = 0x7203E0;
}

void DC2CALL Burner_Normal(gptr o)                                  /* 0x427F10 */
{
    Orig_Call1(U32(BN_MOVE + U8(o + 0x55) * 4), o);
}

/* step 0: off; lit (+0x1C0) after 3 frames */
void DC2CALL Burner_Off(gptr o)                                     /* 0x427C90 */
{
    if (!U16(o + 0x1C0)) return;
    u16 c = U16(o + 0x1CC);
    if (!c) { U16(o + 0x1CC) = 3; return; }
    U16(o + 0x1CC) = --c;
    if ((s16)c > 0) return;
    U16(o + 0x1C2) = 0x96;
    U32(o + 0x50) |= 0xA;
    U16(o + 0x1CC) = 0;
    U8(BN_SLOT(o) + 0x64) = 0xA0;
    U8(BN_SLOT(o) + 0x66) = 0x20;
    U8(BN_SLOT(o) + 0x65) = 0x20;
    U8(o + 0x55)++;
    Se_Play(o, 0x6A);
}

/* a flame light every n frames */
static void burner_light(gptr o, u32 n, u32 rec)
{
    if ((s32)S16(o + 0x1C2) % (s32)n) return;
    gptr l = Light_Alloc();
    if (!l) return;
    U8(l + 1) = 2;
    U32(l + 0x14) = o;
    U32(l + 0xC) = rec;
    Light_Step(l);
}

/* the flame burns what is ahead of it */
static u32 burner_hit(gptr o)
{
    gptr f = Tmp_Push(0x28), r = f, m = f + 8;
    U16(r) = 0x400; U16(r + 2) = U16(o + 0x4A); U16(r + 4) = 0;
    RotMatrixYXZ(r, m);
    u32 h = Weapon_Hit(m, o + 0x40, 0x19);
    Tmp_Pop(0x28);
    return h;
}

/* out: cooling for 300 frames */
static void burner_out(gptr o)
{
    U32(o + 0x50) &= ~0xAu;
    U16(o + 0x1C2) = 0x12C;
    U16(o + 0x1CC) = 0;
    U16(o + 0x1C0) = 2;
    U8(o + 0x55) = 2;
}

/* step 1: burning */
void DC2CALL Burner_Burn(gptr o)                                    /* 0x427D70 */
{
    if (--S16(o + 0x1C2) <= 0) { burner_out(o); Se_StopGroup(0x6A); return; }
    if (U16(o + 0x1C4)) burner_light(o, 8, 0x5ED530);
    else burner_light(o, 7, 0x5ED924);
    burner_hit(o);
}

/* step 2: cooling; off again, its lamp green */
void DC2CALL Burner_Cool(gptr o)                                    /* 0x427E70 */
{
    if (--S16(o + 0x1C2) > 0) return;
    U8(o + 0x55) = 0;
    U16(o + 0x1C0) = 0;
    U16(o + 0x1CC) = 0;
    U8(BN_SLOT(o) + 0x66) = 0x20;
    U8(BN_SLOT(o) + 0x64) = 0x20;
    U8(BN_SLOT(o) + 0x65) = 0xA0;
}

/* state 2: the long burn shown by the camera; then back to state 1 cooling */
void DC2CALL Burner_Show(gptr o)                                    /* 0x427F30 */
{
    if (!U16(o + 0x1CA)) { Se_Play(o, 0x6B); U16(o + 0x1CA) = 1; }
    if (--S16(o + 0x1C2) <= 0) {
        burner_out(o);
        U8(o + 0x54) = 1;
        Se_StopGroup(0x6A); Se_StopGroup(0x6B);
        U16(o + 0x1CA) = 0;
        return;
    }
    if (U16(o + 0x1C4)) burner_light(o, 7, 0x5ED674);
    else burner_light(o, 8, U16(o + 0x1C6) == 5 ? 0x5EDA68 : 0x5ED790);
    if (burner_hit(o)) U32(G() + 0xB54) |= 0x80000000;
}

/* ---- the objects of the player's weapons (types 29/30 of areas 0..8, 28/29
 * in area 6): by the weapon in hand G+0x10B4 (its low 4 bits: 0x72160C; the
 * sub-weapon, its high byte: 0x721640) ---- */
void DC2CALL WeaponObj_Type(gptr o)                                 /* 0x4488C0 */
{
    Orig_Call1(U32(0x72160C + (U8(G() + 0x10B4) & 0xF) * 4), o);
}

/* ---- weapon 1's object (0x72160C [1]): an effect slot of the K2 kind, its
 * kind +0x7A picking a record of 0x7220C8 {u8 flags, u8 tpage, u8 clut, -,
 * u32 animation}; steps 0x722100 (0 set up, 1 fly, 2 K2_Unlink) ---- */

/* step 0: kind 2 only lit; else turned upright, its animation, sprite page and
   CLUT; kind 3 thrown with a random push (bounces: Fly), kind 4 a burst of
   three (two copies made behind it) */
void DC2CALL WeaponObj1_Init(gptr o)                                 /* 0x44E570 */
{
    u8 kind = U8(o + 0x7A);
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0; U16(v + 6) = 0;
    U8(o + 0x54)++;
    if (kind == 2) {
        K2_Light(o, 0);
        U8(o + 0x50) &= 0xFD;
        Tmp_Pop(8);
        return;
    }
    RotMatrix(v, o + 0x20);
    Tmp_Pop(8);
    K2_SetAnim(o, U32(0x7220CC + U8(o + 0x7A) * 8));
    u32 k = U8(o + 0x7A);
    U16(o + 0x6A) = (u16)(U8(0x7220C9 + k * 8) << 5 | 0xA);
    U16(o + 0x68) = (u16)(U8(0x7220CA + k * 8) + 0x7E08);
    U32(o + 0x50) |= 2;
    U8(o + 0x67) |= U8(0x7220C8 + k * 8);
    if (k == 4) {
        U8(o + 0x84) |= 1;
        U8(o + 0x46) = 0;
        U16(o + 0x4C) = 0x400; U16(o + 0x4A) = 0x400;
        gptr a = K2_AllocBack();
        if (!a) { K2_Unlink(o); return; }
        memcpy(MEM(a), MEM(o), 0xAC);
        U16(o + 0x4A) = 0;
        K2_SyncParent(a);
        gptr b = K2_AllocBack();
        if (!b) { K2_Unlink(o); return; }
        memcpy(MEM(b), MEM(o), 0xAC);
        K2_SyncParent(b);
        U16(o + 0x42) += 0x40;
        U16(o + 0x48) = 0x400;
        K2_SetAnim(o, U32(0x7220F4));
    } else if (k == 3) {
        U8(o + 0x85) = 0x10;
        U8(o + 0x46) = 1;
        U16(o + 0x7C) = (u16)((Rand8() >> 4) + 0xC);
        U16(o + 0x7E) = (u16)((Rand8() >> 3) - 0x2E);
        U8(o + 0x7B) = 4;
        U16(o + 0x80) = (u16)((Rand8() >> 3) + 0x48);
        U16(o + 0x9A) = 0xA;
        U16(o + 0x9E) = 0x32;
    }
}

/* step 1: moved by its speed (+0x7C/+0x7E/+0x80, the last position kept at
   +0x74); kinds 0/1 until the animation ends, 2 gone, 3 bounces off the floor
   (losing speed, a sound) and the walls until its time is out, 4.. gone */
void DC2CALL WeaponObj1_Fly(gptr o)                                  /* 0x44E740 */
{
    gptr pos = o + 0x40, last = o + 0x74;
    u32 old = U32(pos);
    U16(o + 0x42) += U16(o + 0x7E);
    U16(o + 0x40) += U16(o + 0x7C);
    U32(last) = old;
    u16 z = U16(o + 0x44);
    U16(o + 0x78) = z;
    U16(o + 0x44) = (u16)(U16(o + 0x80) + z);
    switch (U8(o + 0x7A)) {
    case 0: case 1:
        if (K2_AnimStep(o)) U8(o + 0x54) = 2;
        return;
    case 2:
        U8(G() + 0xF) = 0;
        K2_Unlink(o);
        return;
    case 3: {
        s32 y = S16(o + 0x42);
        U16(o + 0x4C) += 0xFE00;
        U16(o + 0x5A) = (u16)(y / -1000);
        u16 r = (u16)Floor_Cross(pos, last, pos, 0x10);
        if (r == 0xFFFE) { U8(o + 0x54) = 2; return; }
        if (!r) {                                       /* on the floor: back, slower, upward */
            U16(o + 0x42) = U16(o + 0x76);
            U16(o + 0x44) = U16(o + 0x78);
            U16(pos) = U16(last);
            s32 v = -(S16(o + 0x7E) * 3) / 5;
            U16(o + 0x7E) = (u16)v;
            if ((s32)(s16)v + S16(o + 0x9A) > -1) { U16(o + 0x9A) = 0; U16(o + 0x7E) = 0; }
            Se_Play(U32(G() + 0x940), 0x13);
        }
        if (Eff_WallHit(o)) {                           /* a wall: back, turned round */
            U16(pos) = U16(last);
            U16(o + 0x44) = U16(o + 0x78);
            U16(o + 0x80) = (u16)-U16(o + 0x80);
            U16(o + 0x7C) = (u16)-U16(o + 0x7C);
            Se_Play(U32(G() + 0x940), 0x13);
        }
        K2_AnimStep(o);
        if (--S16(o + 0x9E) <= 0 && !--S16(o + 0x6C)) K2_Unlink(o);
        return;
    }
    case 4: case 5: case 6:
        K2_Unlink(o);
        return;
    }
}

void DC2CALL WeaponObj1(gptr o)                                      /* 0x44E530 */
{
    Orig_Call1(U32(0x7215E4 + U8(o + 0x7B) * 4), o);
    Orig_Call1(U32(0x722100 + U8(o + 0x54) * 4), o);
    K2_SyncParent(o);
    if (!--U8(o + 0x46)) K2_Detach(o);
}

void DC2CALL SubWeaponObj_Type(gptr o)                              /* 0x4488E0 */
{
    Orig_Call1(U32(0x721640 + (U16(G() + 0x10B4) >> 8) * 4), o);
}
