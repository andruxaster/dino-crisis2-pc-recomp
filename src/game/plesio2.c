/* The plesiosaurs in the water (object types 11 and 12, E31.DAT / E32.DAT;
 * type handlers 0x42DCC0 and 0x430360). Both wait at the water's surface
 * (their height then stays at o+0x208, where they were placed), come for the
 * player, bite, and go under again; neither wanders far from the others: a
 * new one that is placed within 4000 of another live object is taken away at
 * once and its health is kept for the next (0x74AF80, type 11).
 * Shared state:
 *   +0x1C0 bits (Bits_Set): 0 holds the player, 1 the head looks (type 12),
 *     2 the bite to the side landed, 3 swimming (type 12; it sinks otherwise),
 *     4 strays too far (type 12)
 *   +0x1C4 how it turns about (type 12: 1 either way, 2 left, 3 right)
 *   +0x1C8 the wait after it held the player (type 12)
 *   +0x1CC..0x1CF the state of the last frame (hits go back to it)
 *   +0x1D0 damage taken since the last flinch, +0x1D4 the wait after a
 *     knock-back, +0x1D8 the wait after the player was hurt or held
 *   +0x1E8 where the lunge goes, +0x1EE how long the player has been far,
 *   +0x1F0 its swimming speed's random part, +0x1F2 the leap's doubling,
 *   +0x1F4 the player's last squared distance, +0x1F8 where it held the
 *     player, +0x1FE time swimming, +0x208 the water's height, +0x20A the cry's
 *     wait, +0x20C the dive's wait, +0x210/0x214 two glows (effect slots),
 *     +0x218 its splash (effect), +0x21C the bubbles' timer, +0x220 the wait
 *     before it lets go, +0x222 no hits while set, +0x224 time backing away.
 * States (o+0x54): 0 init, 1 normal (think, then move by o+0x55), 2 hit,
 * 3 die, 4 (0x4A56D0); attacks land through the grab table by o+0x55.
 * Type 11 moves: 0 waits, 1 swims closer, 2 backs away, 3 leaps aside,
 * 4 dives away, 5 rises, 6 lunges, 7 bites in front, 8 the bite to the side,
 * 9 drags the player under, 10 eats the player, 11 dives and rises,
 * 12 snaps. Type 12 has the same moves without the drag/eat ones and with no
 * splashes. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define P11_MAIN  0x720900u
#define P11_THINK 0x720984u
#define P11_MOVE  0x7209B8u
#define P11_HIT   0x7209ECu
#define P11_DIE   0x7209FCu
#define P11_GRAB  0x720A0Cu
#define P12_MAIN  0x720A40u
#define P12_THINK 0x720AC4u
#define P12_MOVE  0x720AF8u
#define P12_HIT   0x720B2Cu
#define P12_DIE   0x720B3Cu
#define P12_GRAB  0x720B4Cu
#define SHARED    0x87E974u     /* bit 0 one holds the player, 2 one died its other way */
#define PL()      U32(G() + 0x940)
#define BITS(o)   ((o) + 0x1C0)

static s32 iabs(s32 v) { return v < 0 ? -v : v; }

static void still(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }

/* the player's x/y/z and heading set to its own */
static void hold_at(gptr o, gptr pl)
{
    U16(pl + 0x40) = U16(o + 0x40); U16(pl + 0x42) = U16(o + 0x42);
    U16(pl + 0x44) = U16(o + 0x44); U16(pl + 0x4A) = U16(o + 0x4A);
}

/* moved along its animation's offset (+0xD0), doubled when +0x1F2 is set */
static void leap_move(gptr o)
{
    gptr m = Tmp_Push(0x20), v = Tmp_Push(8);
    RotMatrix(o + 0x48, m);
    ApplyMatrixSV(m, o + 0xD0, v);
    s32 s = S16(o + 0x1F2);
    U16(o + 0x40) += (u16)(U32(v) << s);
    U16(o + 0x42) += (u16)(U32(v + 2) << s);
    U16(o + 0x44) += (u16)(U32(v + 4) << s);
    Tmp_Pop(8); Tmp_Pop(0x20);
}

/* the object to the player's one before it, down to the first after it */
#define OBJ_LAST() (PL() + U32(0x87F940) * 0x260 - 0x260)

/* another live object within 4000 (x/z); the last one is looked at even
 * when it is the player (one object) */
static int near_other(gptr o, int skip_diving)
{
    gptr p = OBJ_LAST();
    do {
        if (!(U8(p + 0x50) & 1) || p == o) continue;
        if (skip_diving && U8(p + 0x54) == 1 && U8(p + 0x55) == 4) continue;
        if (Dist2_XZ(o + 0x40, p + 0x40) <= 0xF42400) return 1;
    } while ((p -= 0x260) > PL());
    return 0;
}

/* two glows by its head: made (0), grow (1), shrink (2), full (3) or follow
 * the head under the water (4) */
static void glow(gptr o, u32 mode)
{
    switch (mode & 0xFF) {
    case 0: {
        gptr k = K2_AllocBack();
        U32(o + 0x210) = k;
        if (k) {
            U8(k + 0x58) = 3; U8(k + 0x7A) = 0xA; U16(k + 0x6C) = 2; U16(k + 0x6E) = 2;
            U16(k + 0x40) = U16(o + 0x40); U16(k + 0x42) = (u16)(U16(o + 0x42) + 200); U16(k + 0x44) = U16(o + 0x44);
            U16(k + 0x48) = 0x400; U8(k + 0x64) = 0x40; U8(k + 0x65) = 0x40; U8(k + 0x66) = 0x40; U8(k + 0x84) = 1;
        }
        k = K2_AllocBack();
        U32(o + 0x214) = k;
        if (k) {
            U8(k + 0x58) = 3; U8(k + 0x7A) = 0xA; U16(k + 0x6C) = 1; U16(k + 0x6E) = 1;
            U16(k + 0x40) = U16(o + 0x40); U16(k + 0x42) = (u16)(U16(o + 0x42) + 200); U16(k + 0x44) = U16(o + 0x44);
            U16(k + 0x48) = 0x400; U8(k + 0x64) = 0x40; U8(k + 0x65) = 0x40; U8(k + 0x66) = 0x40; U8(k + 0x84) = 1;
        }
        return; }
    case 1: {
        gptr k = U32(o + 0x210);
        if (k) {
            U16(k + 0x6C) += 6; U16(k + 0x6E) += 6;
            if (U16(k + 0x6C) > 100) U16(k + 0x6C) = 100;
            if (U16(k + 0x6E) > 100) U16(k + 0x6E) = 100;
        }
        k = U32(o + 0x214);
        if (k) {
            U16(k + 0x6C) += 3; U16(k + 0x6E) += 3;
            if (U16(k + 0x6C) > 0x46) U16(k + 0x6C) = 0x46;
            if (U16(k + 0x6E) > 0x46) U16(k + 0x6E) = 0x46;
        }
        return; }
    case 2: {
        gptr k = U32(o + 0x210);
        if (k) {
            U16(k + 0x6C) -= 2; U16(k + 0x6E) -= 2;
            if (U16(k + 0x6C) < 6) U16(k + 0x6C) = 6;
            if (U16(k + 0x6E) < 6) U16(k + 0x6E) = 6;
        }
        k = U32(o + 0x214);
        if (k) {
            U16(k + 0x6C)--; U16(k + 0x6E)--;
            if (U16(k + 0x6C) < 3) U16(k + 0x6C) = 3;
            if (U16(k + 0x6E) < 3) U16(k + 0x6E) = 3;
        }
        return; }
    case 3: {
        gptr k = U32(o + 0x210);
        if (k) { U16(k + 0x6C) = 100; U16(k + 0x6E) = 100; }
        k = U32(o + 0x214);
        if (k) { U16(k + 0x6C) = 0x46; U16(k + 0x6E) = 0x46; }
        return; }
    case 4: {
        gptr p = U32(o + 0x64), m = Tmp_Push(0x20), v = Tmp_Push(8);
        U16(v) = 0; U16(v + 2) = 0;
        U16(v + 4) = (U8(o + 0x54) == 1 && U8(o + 0x55) == 4) ? 0x5DC : 0xFF38;
        RotMatrix(o + 0x48, m);
        ApplyMatrixSV(m, v, v);
        U16(v) += U16(p + 0x40); U16(v + 2) += U16(p + 0x42); U16(v + 4) += U16(p + 0x44);
        for (; p; p = U32(p + 0x54)) {
            if (S16(p + 0x42) >= S16(o + 0x208) - 300) continue;
            for (u32 j = 0x210; j <= 0x214; j += 4) {
                gptr k = U32(o + j);
                if (!k) continue;
                Value_Approach(k + 0x40, 0x12C, U16(v));
                Value_Approach(k + 0x44, 0x12C, U16(v + 4));
                U16(k + 0x42) = (u16)(U16(o + 0x208) - 0x12C);
            }
            break;
        }
        Tmp_Pop(8); Tmp_Pop(0x20);
        return; }
    }
}

/* ---- both kinds ---- */

/* turns toward the player */
void DC2CALL Ples_TurnToPlayer(gptr o)                              /* 0x42F4C0 */
{
    Obj_TurnToPoint(o, 0x19, PL() + 0x40);
}

/* the wait after the player was hurt or held: kept up while it is */
u32 DC2CALL Ples_Wait(gptr o)                                       /* 0x42FEA0 */
{
    gptr pl = PL();
    if (U32(o + 0x1D8)) U32(o + 0x1D8)--;
    u8 s = U8(pl + 0x54);
    if (s == 2 || s == 5 || (U8(pl + 0x174) & 1)) {
        U32(o + 0x1D8) = 0x3C;
        if (!U8(G() + 0x108E)) U32(o + 0x1D8) = 0x78;
    }
    return U32(o + 0x1D8);
}

/* the parts out of the water are hidden from the water's effects, those
 * under it darker the deeper they are */
void DC2CALL Ples_PartsWater(gptr o)                                /* 0x42FF10 */
{
    for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54)) {
        s32 w = S16(o + 0x208), y = S16(p + 0x42);
        if (y > w + 300) U32(p + 0x50) |= 0x10000000;
        else U32(p + 0x50) &= ~0x10000000u;
        u8 c = y > w - 200 ? 0x40 : y > w - 400 ? 0x20 : 0x80;
        U8(p + 0x8E) = c; U8(p + 0x8D) = c; U8(p + 0x8C) = c;
    }
}

/* ---- type 11 ---- */

/* the animation set of the player when held: 4 in the later stages */
u32 DC2CALL Ples11_PlayerSet(void)                                  /* 0x42FE80 */
{
    return U8(G() + 0x108F) ? 4 : 0;
}

/* its splash: the old one goes */
void DC2CALL Ples11_Splash(gptr o, gptr d)                          /* 0x42FFC0 */
{
    gptr e = U32(o + 0x218);
    if (e && U32(e + 0xA0) == o) K2_Unlink(e);
    U32(o + 0x218) = Effect_Attach(o, d);
}

void DC2CALL Ples11_Glow(gptr o, u32 mode)                          /* 0x430000 */
{
    glow(o, mode);
}

/* taken away: its health is kept for the next one */
void DC2CALL Ples11_Remove(gptr o)                                  /* 0x42EA20 */
{
    gptr pl = PL();
    for (u32 i = 0; i < 4; i++)
        if (!U16(0x74AF80 + 2 * i)) { U16(0x74AF80 + 2 * i) = U16(o + 0x108); break; }
    U16(pl + 0x1E8) = 0;
    Enemy_BiteOff(o);
    Enemy_Remove(o);
}

void DC2CALL Ples11_Type(gptr o)                                    /* 0x42DCC0 */
{
    Ples11_Handler(o);
}

void DC2CALL Ples11_Handler(gptr o)                                 /* 0x42DCD0 */
{
    gptr g = G(), pl = U32(g + 0x940);
    U8(o + 0x226) = U8(g + 0x108F) ? 1 : 0;
    Obj_SavePos(o);
    Orig_Call1(U32(P11_MAIN + U8(o + 0x54) * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Ples_PartsWater(o);
    Obj_UpdateMatrix(o);
    Ples11_Glow(o, 4);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U16(pl + 0x1FA) > 0x80) Bits_Set(SHARED, 0, 0);
    Ples_Wait(o);
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    if (U32(o + 0x1D4)) U32(o + 0x1D4)--;
    if (U16(o + 0x206)) U16(o + 0x206)--;
    if (U16(o + 0x220)) U16(o + 0x220)--;
    if (U16(o + 0x222)) U16(o + 0x222)--;
    Ples11_Grab(o);
    if (U16(o + 0x20A)) U16(o + 0x20A)--;
    U32(o + 0x1CC) = U32(o + 0x54);
    if (U32(o + 0x50) & 0x40000000) {
        if (U32(o + 0x210)) K2_Unlink(U32(o + 0x210));
        if (U32(o + 0x214)) K2_Unlink(U32(o + 0x214));
        gptr e = U32(o + 0x218);
        if (e && U32(e + 0xA0) == o) K2_Unlink(e);
        U32(o + 0x210) = 0; U32(o + 0x214) = 0; U32(o + 0x218) = 0;
        Bits_Set(SHARED, 2, 0);
        Obj_Disable(o);
    }
}

/* state 0: health, model, animation; a kept health is taken; too near
 * another, taken away at once */
void DC2CALL Ples11_Init(gptr o)                                    /* 0x42DEA0 */
{
    gptr pl = PL();
    U16(o + 0x108) = 0x5DC;
    U8(o + 0x54)++;
    U32(o + 0x50) = 1;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 0xBB8;
    if (!U8(G() + 0x108E)) S16(o + 0x108) >>= 1;
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x12C) = 0x720918;
    U32(o + 0x128) = U8(G() + 0x1091) == 6 && U8(G() + 0x1090) == 1 ? 0x720928 : 0x720938;
    U32(o + 0x134) = 0x720948;
    still(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x640000);
    U32(o + 0x88) = 0x642348;
    Anim_Start(o, 6, 0, 0x40, 5);
    DrawSync(o);
    Head_Reset(o);
    U8(o + 0x15F) = 0xF; U8(o + 0x160) = 0xE;
    U16(o + 0x164) = 0x60; U16(o + 0x166) = 0x60;
    U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    U16(o + 0x150) = 0; U16(o + 0x152) = 0xF5D8; U16(o + 0x154) = 0xBBA4;
    Bits_Set(BITS(o), 1, 1);
    U32(o + 0x54) = 0x501;
    U16(o + 0x208) = U16(o + 0x42);
    U32(o + 0x1F4) = 0;
    U16(o + 0x1F0) = Rand8() & 0x1F;
    U8(o + 0xFE) = 3;
    U32(o + 0x1C4) = Rand8() & 1 ? 0 : Rand8() & 3;
    U32(pl + 0x50) |= 0x80;
    for (u32 i = 0; i < 4; i++)
        if (U16(0x74AF80 + 2 * i)) {
            U16(o + 0x108) = U16(0x74AF80 + 2 * i);
            U16(0x74AF80 + 2 * i) = 0;
            break;
        }
    if (near_other(o, 0)) { Ples11_Remove(o); return; }
    U32(o + 0x218) = 0;
    U16(o + 0x222) = 0x14;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

void DC2CALL Ples11_Normal(gptr o)                                  /* 0x42E160 */
{
    Orig_Call1(U32(P11_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(P11_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Ples11_Hit(gptr o)                                     /* 0x42E190 */
{
    Orig_Call1(U32(P11_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Ples11_Die(gptr o)                                     /* 0x42E1B0 */
{
    Orig_Call1(U32(P11_DIE + U8(o + 0x55) * 4), o);
}

void DC2CALL Ples11_Grab(gptr o)                                    /* 0x42E1D0 */
{
    Enemy_AttackHit(o);
    if ((s8)U8(o + 0x119) >= 0) Orig_Call1(U32(P11_GRAB + U8(o + 0x55) * 4), o);
}

/* looks at the player's head; picks an attack (0), the dive away when another
 * is near or the player far (0) or a leap aside when the player turns its
 * back close by (0); otherwise the player's squared distance */
u32 DC2CALL Ples11_Decide(gptr o)                                   /* 0x42E590 */
{
    gptr pl = PL();
    Obj_TurnToPoint(o, 0x19, Obj_Part(U32(pl + 0x64), 8) + 0x40);
    u32 d = Dist2_XZ(o + 0x40, pl + 0x40);
    if (d > 0x18CE190) U16(o + 0x1EE)++;
    else U16(o + 0x1EE) = 0;
    s16 a = (s16)Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A));
    s16 r = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    if (!Bits_Check(SHARED, 0) && iabs(a) < 0x80 && !U32(o + 0x1D8)) {
        if (d < 0x18CE190 && d > 0x1511110) {
            if (U16(pl + 0x1FA) > Rand8()) {
                u32 side = r < -0x40 ? 1 : 0;
                if (r > 0x40) side = 2;
                Enemy_BiteOn(o);
                U32(o + 0x54) = side << 24 | 0x701;
            } else {
                Enemy_BiteOn(o);
                U32(o + 0x54) = (u32)(r > 0) << 24 | 0x801;
            }
            return 0;
        }
        if (d < 0xBAEB90 && d > 0x77A100) {
            Enemy_BiteOn(o);
            U32(o + 0x54) = (u32)(r > 0) << 24 | 0xC01;
            return 0;
        }
    }
    if (near_other(o, 1) || d > 0x8954400 || S16(o + 0x1EE) > 0x5A) {
        U32(o + 0x54) = 0x401;
        return 0;
    }
    if (!Pos_Hidden(o + 0x40)) Ples11_Remove(o);
    if (d < 0x18CE190 && U8(pl + 0x54) == 1 && U8(pl + 0x55) == 2 && U32(o + 0x1F4) < d) {
        if (r > 0x280 && r < 0x380) { U32(o + 0x54) = 0x301; return 0; }
        if (r < -0x280 && r > -0x380) { U32(o + 0x54) = 0x1000301; return 0; }
    }
    U32(o + 0x1F4) = d;
    return d;
}

/* turned about: the leap to the side the player's heading is */
static void leap_side(gptr o, gptr pl)
{
    U32(o + 0x54) = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A)) > 0 ? 0x301 : 0x1000301;
}

void DC2CALL Ples11_ThinkWait(gptr o)                               /* 0x42E200 */
{
    if (U8(o + 0x56) != 1) return;
    u32 d = Ples11_Decide(o);
    if (!d) return;
    if (d > 0x18CE190) U32(o + 0x54) = 0x101;
    else if (d < 0x1511110) U32(o + 0x54) = 0x201;
}

void DC2CALL Ples11_ThinkSwim(gptr o)                               /* 0x42E330 */
{
    gptr pl = PL();
    u32 d = Ples11_Decide(o);
    if (!d) return;
    if (d < 0x18CE190) { U32(o + 0x54) = 1; return; }
    if (S16(o + 0x1FE) > 0x5A) { U16(o + 0x1FE) = 0; leap_side(o, pl); }
}

void DC2CALL Ples11_ThinkBack(gptr o)                               /* 0x42E470 */
{
    gptr pl = PL();
    u32 d = Ples11_Decide(o);
    if (!d) return;
    if (d > 0x1511110) { U32(o + 0x54) = 1; return; }
    U16(o + 0x224)++;
    if (S16(o + 0x224) > 0x5A) { U16(o + 0x224) = 0; leap_side(o, pl); }
}

/* waits at the surface */
void DC2CALL Ples11_Wait2(gptr o)                                   /* 0x42E240 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 0, 0, 0x41, 10);
        U16(o + 0x42) = U16(o + 0x208);
        Ples11_Splash(o, 0x6578D4);
        return;
    case 1:
        if (Anim_Step(o)) Ples11_Splash(o, 0x6578D4);
        break;
    case 2:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0, 0, 0x41, 0x14);
        U16(o + 0x42) = U16(o + 0x208);
        return;
    case 3:
        if (Anim_Step(o)) U8(o + 0x56) = 1;
        break;
    default:
        return;
    }
    Value_Approach(o + 0xF4, 10, 0);
}

/* swims closer */
void DC2CALL Ples11_Swim(gptr o)                                    /* 0x42E3A0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 1, 0, 0x41, 5);
        U16(o + 0x42) = U16(o + 0x208);
        Ples11_Splash(o, 0x6579F0);
        U16(o + 0x1FE) = 0;
        U16(o + 0x42) = U16(o + 0x208);
        return;
    case 1:
        if (Anim_Step(o)) { Ples11_Splash(o, 0x6579F0); U16(o + 0x42) = U16(o + 0x208); }
        Value_Approach(o + 0xF4, 10, (u16)(U16(o + 0x1F0) + 0x8C));
        U16(o + 0x1FE)++;
        break;
    }
    U16(o + 0x42) = U16(o + 0x208);
}

/* backs away */
void DC2CALL Ples11_Back(gptr o)                                    /* 0x42E4F0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 2, 0, 0x41, 5);
        U16(o + 0x42) = U16(o + 0x208);
        Ples11_Splash(o, 0x657ABC);
        return;
    case 1:
        if (Anim_Step(o)) { Ples11_Splash(o, 0x657ABC); U16(o + 0x42) = U16(o + 0x208); }
        Value_Approach(o + 0xF4, 10, (u32)-0x8C);
        return;
    }
}

/* leaps aside (o+0x57: which side) */
void DC2CALL Ples11_Leap(gptr o)                                    /* 0x42E840 */
{
    static const u32 an[] = {3, 4}, sp[] = {0x657BB0, 0x657CA4};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, an[U8(o + 0x57)], 1, 0x40, 10);
        Ples11_Splash(o, sp[U8(o + 0x57)]);
        U16(o + 0x1F2) = Rand8() & 1;
        break;
    case 1:
        if (Anim_Step(o) || U16(o + 0x92) >= 0x3A) U32(o + 0x54) = 1;
        leap_move(o);
        break;
    }
    Value_Approach(o + 0xF4, 0x14, 0);
}

/* dives away (and is gone), unless it is out of the room's water */
void DC2CALL Ples11_Dive(gptr o)                                    /* 0x42E950 */
{
    switch (U8(o + 0x56)) {
    case 0:
        if (!PointInQuad(o + 0x40, U32(G() + 0x7BC) + 0x1C)) { Ples11_Remove(o); return; }
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 5, 0, 0x40, 10);
        U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x30;
        Ples11_Splash(o, 0x657D98);
        return;
    case 1:
        Value_Approach(o + 0xF4, 10, (u32)-0x8C);
        Ples11_Glow(o, 2);
        if (Anim_Step(o)) Ples11_Remove(o);
        return;
    }
}

/* rises facing the player */
void DC2CALL Ples11_Rise(gptr o)                                    /* 0x42EA80 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 6, 0, 0, 10);
        U16(o + 0x4A) = (u16)Angle_XZ(o + 0x40, pl + 0x40);
        U32(o + 0x50) |= 0xC;
        Ples11_Glow(o, 0);
        Ples11_Splash(o, 0x657FA4);
        return;
    case 1:
        Ples11_Glow(o, 1);
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        return;
    }
}

/* the lunge: to 6200 short of the player; a bite at its end if close and the
 * player is down */
void DC2CALL Ples11_Lunge(gptr o)                                   /* 0x42EB30 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 8, 0, 0x40, 10);
        Ples11_Splash(o, 0x65B61C);
        u32 ang = Angle_XZ(o + 0x40, pl + 0x40);
        U16(o + 0x4A) = (u16)ang;
        gptr v = Tmp_Push(8);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0xE7C8;
        Vec_RotY(v, v, ang);
        U16(o + 0x1EA) = 0;
        U16(o + 0x1E8) = (u16)(U16(v) + U16(pl + 0x40));
        U16(o + 0x1EC) = (u16)(U16(pl + 0x44) + U16(v + 4));
        Tmp_Pop(8);
        return; }
    case 1: {
        gptr v = Tmp_Push(8);
        u32 d = Vec_StepXZ(o + 0x40, o + 0x1E8, 0x8C, v);
        U16(o + 0x42) += U16(v + 2); U16(o + 0x44) += U16(v + 4); U16(o + 0x40) += U16(v);
        Tmp_Pop(8);
        if (!Anim_Step(o)) return;
        if ((U8(pl + 0x174) & 2) && d < 0x1F4) {
            Enemy_BiteOn(o);
            U32(o + 0x54) = S16(pl + 0x108) < 0xFA ? 0xA01 : 0x901;
            return;
        }
        Enemy_BiteOff(o);
        U32(o + 0x54) = 1;
        return; }
    }
}

/* bites in front (o+0x57: high, middle or low) */
void DC2CALL Ples11_Bite(gptr o)                                    /* 0x42ECA0 */
{
    static const u32 an[] = {0xA, 0xB, 0xC}, sp[] = {0x65AB38, 0x65AC2C, 0x65AD48};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, an[U8(o + 0x57)], 0, 0x40, 10);
        still(o);
        Ples11_Splash(o, sp[U8(o + 0x57)]);
        return;
    case 1:
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
        return;
    }
}

/* the bite to the side: lets go when the player gets away */
void DC2CALL Ples11_ThinkSide(gptr o)                               /* 0x42ED60 */
{
    gptr pl = PL();
    Obj_TurnToPoint(o, 0x19, pl + 0x40);
    if (!U8(o + 0x56)) return;
    u8 s = U8(pl + 0x54);
    if (s == 2 || s == 5) return;
    u32 d = Dist2_XZ(o + 0x40, pl + 0x40);
    if ((d > 0x284AF10 || d < 0xDC5640) && !U16(o + 0x220)) {
        Enemy_BiteOff(o);
        U32(o + 0x54) = 0x20001;
    }
}

/* turns to the player, then the bite to the side; a held player is dragged */
void DC2CALL Ples11_Side(gptr o)                                    /* 0x42EDD0 */
{
    static const u32 an[] = {0xD, 0xE, 0xA}, sp[] = {0x65AF2C, 0x65B160, 0x65AB38};
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0x220) = 0x3C;
        if (iabs((s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A))) < 0x400) {
            U8(o + 0x56)++;
            Anim_Start(o, an[U8(o + 0x57)], 1, 0x40, 10);
            Ples11_Splash(o, sp[U8(o + 0x57)]);
        } else Anim_Start(o, 7, 0, 0x40, 10);
        still(o);
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, an[U8(o + 0x57)], 0, 0x40, 10);
            Ples11_Splash(o, sp[U8(o + 0x57)]);
        }
        break;
    case 2:
        if (Anim_Step(o)) {
            if ((U8(pl + 0x174) & 1) && !Flag_Check(2, 0x1A) && Bits_Check(BITS(o), 2)) {
                U32(o + 0x184) |= 1;
                U32(o + 0x54) = 0x601;
                Bits_Set(BITS(o), 2, 0);
            } else {
                Enemy_BiteOff(o);
                U32(o + 0x54) = 1;
            }
        }
        break;
    }
    Value_Approach(o + 0xF4, 0x14, 0);
}

void DC2CALL Ples11_ThinkDrag(gptr o)                               /* 0x42EFA0 */
{
    if (U8(o + 0x56) == 1) Obj_TurnToPoint(o, 0x19, PL() + 0x40);
}

/* the player after it was dragged under: back up */
void DC2CALL Ples11_PlayerUp(gptr pl)                               /* 0x42F2C0 */
{
    if (!Anim_Step(pl)) return;
    Anim_Set(pl, 0xC, 0, 0, 10);
    Obj_UpdateMatrix(pl);
    U32(pl + 0x54) = 0x30202;
}

/* takes hold of the player and drags it under */
void DC2CALL Ples11_Drag(gptr o)                                    /* 0x42EFD0 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U32(o + 0x50) &= ~8u;
        still(o);
        Anim_Start(o, 0xF, 0, 0x40, 10);
        Ples11_Splash(o, 0x65BBEC);
        Flash_Play(o, 0x65C1B8);
        U16(o + 0x1F8) = U16(pl + 0x40); U16(o + 0x1FA) = U16(pl + 0x42); U16(o + 0x1FC) = U16(pl + 0x44);
        Player_Hold(o, 0, 0x48E100);
        return;
    case 1:
        if (!Anim_Step(o)) return;
        U8(o + 0x56)++;
        Anim_Start(o, 0x10, 0, 0x40, 0);
        Ples11_Splash(o, 0x65BCE0);
        Flash_Play(o, 0x65C1CC);
        Anim_Start(pl, U32(U32(o + 0x88) + Ples11_PlayerSet() * 4 + 0x78), 0, 0x40, 0);
        U32(pl + 0x50) |= 0x130;
        Flag_Set(1, 1, 1);
        hold_at(o, pl);
        return;
    case 2:
        hold_at(o, pl);
        if (!Anim_Step(o)) return;
        U8(o + 0x56)++;
        Anim_Start(o, 0x11, 0, 0x40, 0);
        Ples11_Splash(o, 0x65C0F4);
        Anim_Set(pl, U32(U32(o + 0x88) + Ples11_PlayerSet() * 4 + 0x7C), 0, 0x40, 0);
        Obj_UpdateMatrix(pl);
        Flash_Play(pl, 0x65C22C);
        Effect_Attach(pl, Ples11_PlayerSet() ? 0x65A428 : 0x65A21C);
        U16(pl + 0x42) -= 200;
        U16(pl + 0xE0) = U16(o + 0x1F8); U16(pl + 0x40) = U16(o + 0x1F8);
        U16(pl + 0xE4) = U16(o + 0x1FC); U16(pl + 0x44) = U16(o + 0x1FC);
        Obj_UpdateMatrix(pl);
        U32(pl + 0x50) &= ~0x130u;
        Flag_Set(1, 1, 0);
        U32(G() + 0x92C) = 0x42F2C0;
        U16(pl + 0x108) -= (u16)Damage_Scale(0xFA);
        if (S16(pl + 0x108) < 0) GameOver_Start(0);
        Player_Bleed(0x1F4);
        return;
    case 3:
        if (!Anim_Step(o)) return;
        U8(o + 0x56)++;
        Anim_Start(o, 9, 0, 0x40, 10);
        Ples11_Splash(o, 0x65B828);
        U32(o + 0x50) |= 8;
        return;
    case 4:
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
        return;
    }
}

/* takes hold of the player and eats it: the game is over */
void DC2CALL Ples11_Eat(gptr o)                                     /* 0x42F300 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U32(o + 0x50) &= ~8u;
        still(o);
        Anim_Start(o, 0xF, 0, 0x40, 10);
        Ples11_Splash(o, 0x65BBEC);
        Flash_Play(o, 0x65C1B8);
        Player_Hold(o, 0, 0x48E100);
        return;
    case 1:
        if (!Anim_Step(o)) return;
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 0x12, 0, 0x40, 2);
        Flash_Play(o, 0x65C1F4);
        Ples11_Splash(o, 0x658A5C);
        U32(o + 0x50) |= 0x30;
        still(pl);
        Anim_Start(pl, U32(U32(o + 0x88) + Ples11_PlayerSet() * 4 + 0x80), 0, 0x40, 0);
        U32(pl + 0x50) |= 0x130;
        Flag_Set(1, 1, 1);
        hold_at(o, pl);
        return;
    case 2:
        hold_at(o, pl);
        if (!Anim_Step(o)) return;
        U8(o + 0x56)++;
        Ples_PartsWater(pl);
        U32(pl + 0x54) = 6;
        U32(G() + 0x930) = 0x48E100;
        GameOver_Start(0);
        return;
    }
}

/* dives, waits and rises again facing the player */
void DC2CALL Ples11_Resurface(gptr o)                               /* 0x42F4E0 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        still(o);
        U16(o + 0x20C) = 0x3C;
        Se_Play(o, 0x42);
        return;
    case 1:
        if (--U16(o + 0x20C)) return;
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 0x13, 0, 0x40, 10);
        U16(o + 0x4A) = (u16)Angle_XZ(o + 0x40, pl + 0x40);
        U32(o + 0x50) |= 0xC;
        Ples11_Glow(o, 0);
        Ples11_Splash(o, 0x659028);
        return;
    case 2:
        Ples11_Glow(o, 1);
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
        Value_Approach(o + 0xF4, 0x14, 0x32);
        return;
    }
}

/* snaps (o+0x57: which way); a held player is dragged */
void DC2CALL Ples11_Snap(gptr o)                                    /* 0x42F5F0 */
{
    static const u32 an[] = {0x14, 0x15}, sp[] = {0x6592D4, 0x6594E0};
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, an[U8(o + 0x57)], 0, 0x40, 10);
        Ples11_Splash(o, sp[U8(o + 0x57)]);
        return;
    case 1:
        if (!Anim_Step(o)) return;
        if ((U8(pl + 0x174) & 1) && !Flag_Check(2, 0x1A)) {
            U32(o + 0x184) |= 1;
            U32(o + 0x54) = 0x601;
            return;
        }
        Enemy_BiteOff(o);
        U32(o + 0x54) = 1;
        return;
    }
}

/* the attacks land */
void DC2CALL Ples11_BiteHit(gptr o)                                 /* 0x42F6E0 */
{
    static const u32 bl[] = {0x658278, 0x658394, 0x658550};
    if (U8(o + 0x56) != 1) return;
    Player_Hurt(o, 0x78, 1, 0);
    Flash_Play(o, 0x65C198);
    Effect_Attach(o, bl[U8(o + 0x57)]);
}

void DC2CALL Ples11_SideHit(gptr o)                                 /* 0x42F770 */
{
    static const u32 bl[] = {0x6586E4, 0x6588C8};
    gptr pl = PL();
    if (U8(o + 0x56) != 2) return;
    if (iabs((s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A))) < 0x400) {
        Player_Hurt(o, 0xDC, 2, 0x28);
        Bits_Set(BITS(o), 2, 1);
    } else Player_Hurt(o, 0xB4, 1, 0);
    Flash_Play(o, 0x65C1A8);
    Effect_Attach(o, bl[U8(o + 0x57)]);
}

void DC2CALL Ples11_ResurfaceHit(gptr o)                            /* 0x42F820 */
{
    if (U8(o + 0x56) != 2) return;
    Player_Hurt(o, 0xB4, 1, 0);
    Flash_Play(o, 0x65C198);
    Effect_Attach(o, 0x65B394);
}

void DC2CALL Ples11_SnapHit(gptr o)                                 /* 0x42F890 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    if (iabs((s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A))) < 0x400) {
        Player_Hurt(o, 0x78, 2, 0x28);
        Flash_Play(o, 0x65C1A8);
    } else {
        Player_Hurt(o, 0x50, 1, 0);
        Flash_Play(o, 0x65C198);
    }
}

/* state 2 step 0: the damage; a flinch, a knock-back or a stagger, or (out
 * of health) death; a held player is let go */
void DC2CALL Ples11_Damage(gptr o)                                  /* 0x42F910 */
{
    static const u32 bl[] = {0x659E28, 0x659EF4, 0x65A038};
    if (U8(o + 0x184) & 8) U16(o + 0x176) += U16(o + 0x176) >> 1;
    u16 how = U16(o + 0x172);
    if (how == 4) U16(o + 0x176) <<= 1;
    U32(o + 0x54) = U32(o + 0x1CC);
    U32(o + 0x1D0) += U16(o + 0x176);
    if ((s32)U32(o + 0x1D0) > S16(o + 0x10A) / 2) { U32(o + 0x1D0) = 0; U32(o + 0x54) = 0x102; }
    if (how == 3) U32(o + 0x54) = 0x202;
    if (how == 4 && (Rand8() < 0x80 || U8(o + 0x54) != 2) && !U32(o + 0x1D4)) {
        U32(o + 0x1D4) = 0x14;
        U32(o + 0x54) = 0x302;
    }
    if (U16(o + 0x222)) U32(o + 0x54) = U32(o + 0x1CC);
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        still(o);
        gptr e = U32(o + 0x218);
        if (e && U32(e + 0xA0) == o) K2_Unlink(e);
        Enemy_BiteOff(o);
        Ples11_Glow(o, 3);
        Bits_Set(BITS(o), 2, 0);
        Bits_Set(BITS(o), 1, 0);
        for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54)) U32(p + 0x50) &= ~0x10000000u;
        if (U8(o + 0x184) & 8) Score_Event(o);
        U16(o + 0x108) -= U16(o + 0x176);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 5 : 10;
        if (S16(o + 0x108) < 0) {
            U32(o + 0x50) &= ~8u;
            U16(o + 0x108) = 0xFFFF;
            U32(o + 0x54) = (u32)U8(o + 0x55) << 8 | 3;
            Score_Kill(o);
        }
    } else {
        U16(o + 0x108) -= (u16)(U16(o + 0x176) / 5);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 5 : 10;
        Effect_Attach(o, bl[Rand8() % 3]);
        if (S16(o + 0x108) <= 0) U16(o + 0x108) = 1;
    }
    Orig_Call1(U32(P11_MAIN + U8(o + 0x54) * 4), o);
}

void DC2CALL Ples11_Flinch(gptr o)                                  /* 0x42FBD0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x16, 0, 0x40, 6);
        Obj_UpdateMatrix(o);
        Effect_Attach(o, 0x6596C4);
        return;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        return;
    }
}

void DC2CALL Ples11_Knock(gptr o)                                   /* 0x42FC30 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x17, 0, 0x40, 3);
        Obj_UpdateMatrix(o);
        Effect_Attach(o, 0x6597B8);
        return;
    case 1:
        Value_Approach(o + 0xF4, 10, 0xC8);
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        return;
    }
}

/* staggers (one of three), with a cry now and then */
void DC2CALL Ples11_Stagger(gptr o)                                 /* 0x42FCA0 */
{
    static const u32 an[] = {0x1A, 0x1B, 0x1C}, bl[] = {0x659E28, 0x659EF4, 0x65A038};
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        u32 k = Rand8() % 3;
        Anim_Start(o, an[k], 0, 0x40, 2);
        if (S16(o + 0x20A) <= 0) {
            Se_Play(o, Rand8() & 1 ? 0x3E : 0x3F);
            U16(o + 0x20A) = 10;
        }
        Obj_UpdateMatrix(o);
        Effect_Attach(o, bl[k]);
        return; }
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        return;
    }
}

/* dies: sinks; one of the two deaths only one at a time */
void DC2CALL Ples11_Dead(gptr o)                                    /* 0x42FD90 */
{
    Obj_DeathFade(o);
    Value_Approach(o + 0xF4, 10, 10);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        if (!(Rand8() & 1) && !Bits_Check(SHARED, 2)) {
            Anim_Start(o, 0x19, 0, 0x40, 3);
            Ples11_Splash(o, 0x65A634);
        } else {
            Anim_Start(o, 0x18, 0, 0x40, 3);
            Ples11_Splash(o, 0x6598AC);
        }
        Bits_Set(SHARED, 2, 1);
        U32(o + 0x50) |= 0x30;
        return;
    case 1:
        U16(o + 0x42) += 10;
        if (U16(o + 0x92) > 0x3C) Ples11_Glow(o, 2);
        if (Anim_Step(o)) { U8(o + 0x56)++; Layers_Assign(); }
        return;
    case 2:
        U32(o + 0x50) |= 0x40000000;
        return;
    }
}

/* ---- type 12 ---- */

void DC2CALL Ples12_Glow(gptr o, u32 mode)                          /* 0x431B60 */
{
    glow(o, mode);
}

/* holds the player: the wait after is shorter for a second holder */
void DC2CALL Ples12_BiteOn(gptr o)                                  /* 0x431A70 */
{
    Enemy_BiteOn(o);
    U16(o + 0x1C8) = Bits_Check(SHARED, 1) ? 0x3C : 0x5A;
}

/* lets go; the next turn about picked again */
void DC2CALL Ples12_BiteOff(gptr o)                                 /* 0x431AD0 */
{
    Enemy_BiteOff(o);
    Bits_Set(BITS(o), 4, 0);
    U32(o + 0x1C4) = Rand8() & 1 ? 0 : Rand8() & 3;
}

/* bubbles from its glow now and then */
void DC2CALL Ples12_Bubbles(gptr o)                                 /* 0x431EC0 */
{
    if (++U32(o + 0x21C) < 4) return;
    U32(o + 0x21C) = 0;
    gptr k = K2_AllocBack();
    if (!k) return;
    U8(k + 0x7A) = 5; U8(k + 0x58) = 0x10;
    U16(k + 0x7C) = 0; U16(k + 0x7E) = 0; U16(k + 0x80) = 0;
    U16(k + 0x6C) = (u16)(Rand8() % 15 + 0x1E);
    U16(k + 0x6E) = (u16)(Rand8() % 15 + 0x1E);
    gptr g = U32(o + 0x210);
    U16(k + 0x40) = U16(g + 0x40); U16(k + 0x42) = U16(g + 0x42); U16(k + 0x44) = U16(g + 0x44);
    U16(k + 0x6A) = 0x3B; U16(k + 0x68) = 0x7E40;
}

/* taken away: its health is kept */
void DC2CALL Ples12_Remove(gptr o)                                  /* 0x430E50 */
{
    gptr pl = PL();
    for (u32 i = 0; i < 4; i++)
        if (!U16(0x74AF90 + 2 * i)) { U16(0x74AF90 + 2 * i) = U16(o + 0x108); break; }
    U16(pl + 0x1E8) = 0;
    Ples12_BiteOff(o);
    Enemy_Remove(o);
}

void DC2CALL Ples12_Type(gptr o)                                    /* 0x430360 */
{
    Ples12_Handler(o);
}

void DC2CALL Ples12_Handler(gptr o)                                 /* 0x430370 */
{
    gptr b = BITS(o);
    U8(o + 0x226) = U8(G() + 0x108F) ? 1 : 0;
    if (U8(o + 0x17B)) U8(o + 0x17B)--;
    Bits_Set(b, 3, 0);
    Obj_SavePos(o);
    Orig_Call1(U32(P12_MAIN + U8(o + 0x54) * 4), o);
    if (!Bits_Check(b, 3)) U16(o + 0x44) += 100;
    Ples12_Bubbles(o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Ples_PartsWater(o);
    U32(o + 0x158) = Bits_Check(b, 1) ? 3 : 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    Ples12_Glow(o, 4);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    Ples_Wait(o);
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    if (U32(o + 0x1D4)) U32(o + 0x1D4)--;
    if (U16(o + 0x206)) U16(o + 0x206)--;
    if (U16(o + 0x220)) U16(o + 0x220)--;
    if (U16(o + 0x222)) U16(o + 0x222)--;
    if (U16(o + 0x1C8)) U16(o + 0x1C8)--;
    Ples12_Grab(o);
    if (U16(o + 0x20A)) U16(o + 0x20A)--;
    U32(o + 0x1CC) = U32(o + 0x54);
    if (U32(o + 0x50) & 0x40000000) {
        if (U32(o + 0x210)) K2_Unlink(U32(o + 0x210));
        if (U32(o + 0x214)) K2_Unlink(U32(o + 0x214));
        U32(o + 0x210) = 0; U32(o + 0x214) = 0; U32(o + 0x218) = 0;
        Bits_Set(SHARED, 2, 0);
        Obj_Disable(o);
    }
}

/* state 0: health (1 on the easiest level), model, animation; too near
 * another, taken away at once */
void DC2CALL Ples12_Init(gptr o)                                    /* 0x430590 */
{
    gptr pl = PL();
    U16(o + 0x108) = 0x4B0;
    U8(o + 0x54)++;
    U32(o + 0x50) = 1;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 0x960;
    if (!U8(G() + 0x108E)) U16(o + 0x108) = 1;
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x12C) = 0x720A58;
    U32(o + 0x128) = U8(G() + 0x1091) == 6 && U8(G() + 0x1090) == 1 ? 0x720A68 : 0x720A78;
    U32(o + 0x134) = 0x720A88;
    still(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x640000);
    U32(o + 0x88) = 0x644924;
    Anim_Start(o, 5, 0, 0x40, 5);
    DrawSync(o);
    Head_Reset(o);
    U8(o + 0x15F) = 0xF; U8(o + 0x160) = 0xE;
    U16(o + 0x164) = 0x60; U16(o + 0x166) = 0x60;
    U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    U16(o + 0x150) = 0; U16(o + 0x152) = 0xF5D8; U16(o + 0x154) = 0xBBA4;
    Bits_Set(BITS(o), 1, 1);
    U32(o + 0x54) = 0x501;
    U16(o + 0x208) = U16(o + 0x42);
    U32(o + 0x1F4) = 0;
    U16(o + 0x1F0) = Rand8() & 0x7F;
    U8(o + 0xFE) = 3;
    U32(o + 0x1C4) = Rand8() & 1 ? 0 : Rand8() & 3;
    U32(pl + 0x50) |= 0x80;
    U32(0x74AF90) = 0; U32(0x74AF94) = 0;
    if (near_other(o, 0)) { Ples12_Remove(o); return; }
    U32(o + 0x218) = 0;
    U16(o + 0x222) = 0x14;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

void DC2CALL Ples12_Normal(gptr o)                                  /* 0x430820 */
{
    Orig_Call1(U32(P12_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(P12_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Ples12_Hit(gptr o)                                     /* 0x430850 */
{
    Orig_Call1(U32(P12_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Ples12_Die(gptr o)                                     /* 0x430870 */
{
    Orig_Call1(U32(P12_DIE + U8(o + 0x55) * 4), o);
}

void DC2CALL Ples12_Grab(gptr o)                                    /* 0x430890 */
{
    Enemy_AttackHit(o);
    if ((s8)U8(o + 0x119) >= 0) Orig_Call1(U32(P12_GRAB + U8(o + 0x55) * 4), o);
}

/* looks at the player; the bite when close and in front (0); turns about now
 * and then, and notes when it has strayed far; otherwise the player's squared
 * distance */
u32 DC2CALL Ples12_Decide(gptr o)                                   /* 0x430B00 */
{
    gptr pl = PL(), b = BITS(o);
    Obj_TurnToPoint(o, 0x19, pl + 0x40);
    u32 d = Dist2_XZ(o + 0x40, pl + 0x40);
    if (d > 0x2ACF790) U16(o + 0x1EE)++;
    else U16(o + 0x1EE) = 0;
    s16 a = (s16)Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A));
    s16 r = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    if (!Bits_Check(SHARED, 0) && iabs(a) < 0x80 && !U32(o + 0x1D8) && d < 0x2EBAE40) {
        Ples12_BiteOn(o);
        U32(o + 0x54) = (u32)(r > 0) << 24 | 0x801;
        return 0;
    }
    if (!Bits_Check(b, 4)) {
        switch (U32(o + 0x1C4)) {
        case 1: U32(o + 0x54) = Rand8() & 1 ? 0x301 : 0x1000301; break;
        case 2: U32(o + 0x54) = 0x301; break;
        case 3: U32(o + 0x54) = 0x1000301; break;
        }
    }
    if (iabs(S16(o + 0x40)) > 0x4650 || S16(o + 0x44) < -5000) Bits_Set(b, 4, 1);
    return d;
}

void DC2CALL Ples12_ThinkWait(gptr o)                               /* 0x4308C0 */
{
    if (U8(o + 0x56) == 1 && !Ples12_Decide(o)) return;
    U32(o + 0x54) = 0x101;
}

void DC2CALL Ples12_ThinkSwim(gptr o)                               /* 0x4309D0 */
{
    gptr pl = PL();
    if (!Ples12_Decide(o)) return;
    if (S16(o + 0x1FE) > 0x5A) { U16(o + 0x1FE) = 0; leap_side(o, pl); }
}

void DC2CALL Ples12_Wait2(gptr o)                                   /* 0x4308F0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0, 0, 0x41, 10);
        U16(o + 0x42) = U16(o + 0x208);
        return;
    case 1:
        break;
    case 2:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0, 0, 0x41, 0x14);
        U16(o + 0x42) = U16(o + 0x208);
        return;
    case 3:
        if (Anim_Step(o)) U8(o + 0x56) = 1;
        Value_Approach(o + 0xF4, 10, 0);
        return;
    default:
        return;
    }
    Anim_Step(o);
    Value_Approach(o + 0xF4, 10, 0);
}

void DC2CALL Ples12_Swim(gptr o)                                    /* 0x430A30 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 1, 0, 0x41, 5);
        U16(o + 0x42) = U16(o + 0x208);
        U16(o + 0x1FE) = 0;
        U16(o + 0x42) = U16(o + 0x208);
        return;
    case 1:
        if (Anim_Step(o)) U16(o + 0x42) = U16(o + 0x208);
        Value_Approach(o + 0xF4, 10, (u16)(U16(o + 0x1F0) + 0xFA));
        U16(o + 0x1FE)++;
        break;
    }
    U16(o + 0x42) = U16(o + 0x208);
}

/* leaps aside, swimming */
void DC2CALL Ples12_Leap(gptr o)                                    /* 0x430C40 */
{
    static const u32 an[] = {2, 3};
    gptr b = BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, an[U8(o + 0x57)], 1, 0x40, 10);
        U16(o + 0x1F2) = Rand8() & 1;
        Bits_Set(b, 1, 1);
        break;
    case 1:
        if (Anim_Step(o) || U16(o + 0x92) >= 0x3A) { U32(o + 0x54) = 1; Bits_Set(b, 1, 0); }
        leap_move(o);
        break;
    }
    Value_Approach(o + 0xF4, 0x28, 0x82);
    Bits_Set(b, 3, 1);
}

void DC2CALL Ples12_Dive(gptr o)                                    /* 0x430D80 */
{
    switch (U8(o + 0x56)) {
    case 0:
        if (!PointInQuad(o + 0x40, U32(G() + 0x7BC) + 0x1C)) { Ples12_Remove(o); return; }
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 4, 0, 0x40, 10);
        U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x30;
        return;
    case 1:
        Value_Approach(o + 0xF4, 10, (u32)-0xFA);
        Ples12_Glow(o, 2);
        if (Anim_Step(o)) Ples12_Remove(o);
        return;
    }
}

void DC2CALL Ples12_Rise(gptr o)                                    /* 0x430EB0 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 5, 0, 0, 10);
        U16(o + 0x4A) = (u16)Angle_XZ(o + 0x40, pl + 0x40);
        U32(o + 0x50) |= 0xA;
        Ples12_Glow(o, 0);
        return;
    case 1:
        Ples12_Glow(o, 1);
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        Value_Approach(o + 0xF4, 10, (u16)(U16(o + 0x1F0) + 0xFA));
        return;
    }
}

void DC2CALL Ples12_Lunge(gptr o)                                   /* 0x430F80 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 7, 0, 0x40, 10);
        u32 ang = Angle_XZ(o + 0x40, pl + 0x40);
        U16(o + 0x4A) = (u16)ang;
        gptr v = Tmp_Push(8);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0xE7C8;
        Vec_RotY(v, v, ang);
        U16(o + 0x1EA) = 0;
        U16(o + 0x1E8) = (u16)(U16(v) + U16(pl + 0x40));
        U16(o + 0x1EC) = (u16)(U16(pl + 0x44) + U16(v + 4));
        Tmp_Pop(8);
        return; }
    case 1: {
        gptr v = Tmp_Push(8);
        u32 d = Vec_StepXZ(o + 0x40, o + 0x1E8, 0xFA, v);
        U16(o + 0x42) += U16(v + 2); U16(o + 0x44) += U16(v + 4); U16(o + 0x40) += U16(v);
        Tmp_Pop(8);
        if (!Anim_Step(o)) return;
        if ((U8(pl + 0x174) & 2) && d < 0x1F4) {
            Ples12_BiteOn(o);
            U32(o + 0x54) = S16(pl + 0x108) < 4 ? 0xA01 : 0x901;
            return;
        }
        Ples12_BiteOff(o);
        U32(o + 0x54) = 1;
        return; }
    }
}

void DC2CALL Ples12_Bite(gptr o)                                    /* 0x4310E0 */
{
    static const u32 an[] = {0xA, 0xB, 0xC};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, an[U8(o + 0x57)], 1, 0x40, 10);
        break;
    case 1:
        if (Anim_Step(o)) { Ples12_BiteOff(o); U32(o + 0x54) = 1; }
        break;
    }
    Bits_Set(BITS(o), 3, 1);
}

/* the bite to the side; held up a while after it last held the player */
void DC2CALL Ples12_Side(gptr o)                                    /* 0x431190 */
{
    static const u32 an[] = {0xD, 0xE, 0xA};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0x220) = 0x3C;
        if (S16(o + 0x1C8) > 0x3C) Anim_Start(o, 6, 0, 0x40, 10);
        else {
            U8(o + 0x56)++;
            Anim_Start(o, an[U8(o + 0x57)], 0, 0x40, 10);
        }
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, an[U8(o + 0x57)], 0, 0x40, 10);
        }
        break;
    case 2:
        if (Anim_Step(o)) { Ples12_BiteOff(o); U32(o + 0x54) = 1; }
        break;
    }
    Bits_Set(BITS(o), 3, 1);
    Value_Approach(o + 0xF4, 0x14, 0);
}

void DC2CALL Ples12_Resurface(gptr o)                               /* 0x4312D0 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        still(o);
        U16(o + 0x20C) = 0x3C;
        Se_Play(o, 0x42);
        return;
    case 1:
        if (--U16(o + 0x20C)) return;
        U8(o + 0x56)++;
        still(o);
        Anim_Start(o, 0xF, 0, 0x40, 10);
        U16(o + 0x4A) = (u16)Angle_XZ(o + 0x40, pl + 0x40);
        U32(o + 0x50) |= 0xA;
        Ples12_Glow(o, 0);
        return;
    case 2:
        Ples12_Glow(o, 1);
        if (Anim_Step(o)) { Ples12_BiteOff(o); U32(o + 0x54) = 1; }
        Value_Approach(o + 0xF4, 0x14, 0xC8);
        return;
    }
}

void DC2CALL Ples12_BiteHit(gptr o)                                 /* 0x4313E0 */
{
    if (U8(o + 0x56) != 1) return;
    Player_Hurt(o, 2, 1, 0);
    Flash_Play(o, 0x65C670);
}

void DC2CALL Ples12_SideHit(gptr o)                                 /* 0x431440 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 2) return;
    if (iabs((s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A))) < 0x400) {
        Player_Hurt(o, 3, 2, 0x28);
        Bits_Set(BITS(o), 2, 1);
    } else Player_Hurt(o, 3, 1, 0);
    Flash_Play(o, 0x65C680);
}

void DC2CALL Ples12_ResurfaceHit(gptr o)                            /* 0x4314D0 */
{
    if (U8(o + 0x56) != 2) return;
    Player_Hurt(o, 3, 1, 0);
    Flash_Play(o, 0x65C670);
}

void DC2CALL Ples12_SnapHit(gptr o)                                 /* 0x431530 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    if (iabs((s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A))) < 0x400) {
        Player_Hurt(o, 2, 2, 0x28);
        Flash_Play(o, 0x65C680);
    } else {
        Player_Hurt(o, 1, 1, 0);
        Flash_Play(o, 0x65C670);
    }
}

/* state 2 step 0: the damage; a flinch or a knock-back (the damage then
 * halved); a stun when it has no grenade weakness left; death */
void DC2CALL Ples12_Damage(gptr o)                                  /* 0x4315B0 */
{
    gptr b = BITS(o);
    if (U8(o + 0x184) & 8) U16(o + 0x176) += U16(o + 0x176) >> 1;
    u16 how = U16(o + 0x172);
    if (how == 4) U16(o + 0x176) <<= 1;
    u16 dmg = U16(o + 0x176);
    U32(o + 0x1D0) += dmg;
    if ((s32)U32(o + 0x1D0) > S16(o + 0x10A) / 2) { U32(o + 0x1D0) = 0; U32(o + 0x54) = 0x102; }
    else { U16(o + 0x176) = dmg >> 1; U32(o + 0x54) = 0x302; }
    if (!U8(o + 0x17C) && !U16(o + 0x206)) {
        U32(o + 0x54) = 0x202;
        U16(o + 0x108) = 0xFFFF;
        U16(o + 0x176) = 0;
    }
    if (U16(o + 0x222)) U32(o + 0x54) = U32(o + 0x1CC);
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        Ples12_BiteOff(o);
        Ples12_Glow(o, 3);
        Bits_Set(b, 2, 0);
        Bits_Set(b, 1, 0);
        Bits_Set(b, 4, 0);
        U32(o + 0x1C4) = Rand8() & 1 ? 0 : Rand8() & 3;
        for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54)) U32(p + 0x50) &= ~0x10000000u;
        U16(o + 0x108) -= U16(o + 0x176);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 5 : 10;
        if (S16(o + 0x108) < 0) {
            U32(o + 0x50) &= ~8u;
            U16(o + 0x108) = 0xFFFF;
            U32(o + 0x54) = (u32)U8(o + 0x55) << 8 | 3;
            Combo_Kill(U8(o + 0x58));
        }
    } else {
        U16(o + 0x108) -= (u16)(U16(o + 0x176) / 5);
        U8(o + 0x17B) = how == 4 ? 5 : 10;
        if (S16(o + 0x108) <= 0) U16(o + 0x108) = 1;
    }
    Orig_Call1(U32(P12_MAIN + U8(o + 0x54) * 4), o);
}

void DC2CALL Ples12_Flinch(gptr o)                                  /* 0x431810 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x10, 0, 0x40, 6);
        Obj_UpdateMatrix(o);
        return;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        return;
    }
}

void DC2CALL Ples12_Knock(gptr o)                                   /* 0x431860 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x11, 0, 0x40, 3);
        Obj_UpdateMatrix(o);
        return;
    case 1:
        Value_Approach(o + 0xF4, 10, 0x258);
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        return;
    }
}

/* stunned: one frame of one of three, with a cry now and then */
void DC2CALL Ples12_Stun(gptr o)                                    /* 0x4318C0 */
{
    static const u32 an[] = {0x14, 0x15, 0x16};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, an[Rand8() % 3], 0, 0x40, 2);
        if (S16(o + 0x20A) <= 0) {
            Se_Play(o, Rand8() & 1 ? 0x3E : 0x3F);
            U16(o + 0x20A) = 10;
        }
        Obj_UpdateMatrix(o);
        return;
    case 1:
        U32(o + 0x54) = 1;
        return;
    }
}

/* dies: sinks; when stunned (+0x17C) a shorter death */
void DC2CALL Ples12_Dead(gptr o)                                    /* 0x431970 */
{
    Obj_DeathFade(o);
    U16(o + 0x44) += 100;
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        still(o);
        if (!U8(o + 0x17C)) Anim_Start(o, 0x12, 1, 0x40, 3);
        else Anim_Start(o, 0x13, 0, 0x40, 3);
        U32(o + 0x50) |= 0x30;
        return;
    case 1:
        U16(o + 0x42) += 10;
        if (U16(o + 0x92) > 0x3C) Ples12_Glow(o, 2);
        if ((!U8(o + 0x17C) && U16(o + 0x92) >= 0x4F) || Anim_Step(o)) { U8(o + 0x56)++; Layers_Assign(); }
        return;
    case 2:
        U32(o + 0x50) |= 0x40000000;
        return;
    }
}
