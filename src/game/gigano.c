/* The giganotosaurus (object type handler 0x4339B0, type 6, E40.DAT). It
 * keeps its state at o+0x1C0.., a set of bits first (Bits_Set(o+0x1C0, n)):
 * 0 holds the bite, 1 moving (its head looks), 2 the attack hits (no
 * stagger), 3 the roar holds the player, 4 the roar's stun frames, 5 a
 * combo of attacks (6/7/8 which: after a threat, after a bite, the short
 * one), 9 the bridge scene began, 0xA it walks the bridge, 0xB it caught the
 * player on it, 0xC the bridge's grab frames, 0xD angry (from the start in
 * the extra mode, or after the bridge), 0xE the player crossed the bridge.
 * Then +0x4 kind (0 normal, 1 the bridge one in room 503, 2 the extra mode),
 * +0x8..0xB the state of the last frame, +0xC damage taken toward a
 * flinch, +0x10 the wait after the player was hurt or held, +0x14 damage
 * taken during a combo, +0x22 its turn speed, +0x24 the point it walks to
 * on the bridge, +0x2A the drop's speed, +0x2C..+0x38 timers (+0x30 the
 * combo's choice, +0x32 attacks left in it), +0x3A the player's side.
 * Bits shared by the giganotosauri at 0x87E974: 0 one bites, 0x1D/0x1E the
 * kind-0 one went down, 0x1F one is down.
 * States (o+0x54): 0 init, 1 normal (think 0x720D5C, then move 0x720D98 by
 * o+0x55), 2 hit (0x720DD4), 3 die (0x720DE4), 4 (0x4A56D0). Attacks land
 * through 0x720DF4.
 * Moves (o+0x55): 0 walk, 1 stand, 2 run, 3 turn from standing, 4 roar (the
 * player stunned), 5 the threat, 6 shake its head, 7/8 bites, 9 swipe, 10
 * snatch the player, 11 maul, 12 lunge, 13 charge, 14 smash the bridge. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define GI_MAIN  0x720D48u      /* state handlers */
#define GI_THINK 0x720D5Cu      /* state 1: deciding, by o+0x55 */
#define GI_MOVE  0x720D98u      /* state 1: moving, by o+0x55 */
#define GI_HIT   0x720DD4u      /* state 2 */
#define GI_DIE   0x720DE4u      /* state 3 */
#define GI_GRAB  0x720DF4u      /* an attack landed, by o+0x55 */
#define GI_BRIDGE 0x720D3Au     /* the bridge's sections: quads of 16 bytes, z of the first point */
#define GI_BITS(o) ((o) + 0x1C0)
#define SHARED   0x87E974u
#define PL()     U32(G() + 0x940)
#define DIST(o)  U32((o) + 0x13C)
#define TURN(o)  U16((o) + 0x1E2)

static s32 iabs(s32 v) { return v < 0 ? -v : v; }
static void stop(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }
static s32 ang_to(gptr o) { return (s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A)); }
static void face(gptr o) { Obj_TurnToPoint(o, TURN(o), o + 0xD8); }
static void quake(gptr o) { Shake_Set(2, 0, 0x1E); Flash_Play(o, 0x65B284); }
static void step(gptr o, u32 se) { quake(o); Se_Play(o, se); }
static void prev_state(gptr o) { U32(o + 0x54) = U32(o + 0x1C8); }

/* the player's damage scale byte by kind */
static u32 kind_byte(gptr o)
{
    u32 k = U32(o + 0x1C4);
    return k == 0 ? 0x64 : k <= 2 ? 0x32 : 0;
}

/* back to walking (kinds 1/2) or the turn (kind 0) */
static void next_move(gptr o, u32 walk)
{
    u32 k = U32(o + 0x1C4);
    if (k == 0) U32(o + 0x54) = 0x301;
    else if (k <= 2) U32(o + 0x54) = walk;
}

/* the player placed on o */
static void place_player(gptr o, gptr pl)
{
    U16(pl + 0x40) = U16(o + 0x40); U16(pl + 0x42) = U16(o + 0x42); U16(pl + 0x44) = U16(o + 0x44);
    U16(pl + 0x4A) = U16(o + 0x4A);
}

/* a prop on the side it was hit from */
static void hit_side_prop(gptr o)
{
    if (!(U8(o + 0x170) & 2) || U8(o + 0x17D) == 0x19) return;
    s16 d = (s16)Angle_Diff((U16(o + 0x178) - 0x800) & 0xFFF, U16(o + 0x4A));
    Prop_Attach(o, d > 0 ? 0x65AB44 : 0x65AD28);
}

/* the player falls through the bridge with it (G+0x92C 0x435B00) */
static void bridge_catch(gptr o, gptr pl)
{
    Player_Hold(o, 0, 0x48E100);
    Anim_Start(pl, U32(U32(o + 0x88) + 0x88), 0xA, 0x40, 0);
    U32(G() + 0x92C) = 0x435B00;
    U16(o + 0x1EA) = 0;
    Obj_ShadowOff(pl);
    Flag_Set(1, 1, 1);
}

/* ---- the handler ---- */

void DC2CALL Gig_Handler(gptr o)                                    /* 0x4339B0 */
{
    gptr pl = PL(), b = GI_BITS(o);
    U8(o + 0x1FA) = U8(G() + 0x108F) ? 1 : 0;
    Obj_SavePos(o);
    u32 k = U32(o + 0x1C4);
    if (k == 1) U16(o + 0x108) = 30000;
    if (k == 0) TURN(o) = 0xF;
    else if (k <= 2) TURN(o) = 0x14;
    if (Bits_Check(b, 5)) TURN(o) = 0x1E;
    u8 m = U8(o + 0x55);
    if (m == 0 || m == 1 || m == 2 || m == 9 || m == 11 || m == 14) U8(o + 0x11B) = 0;
    else U8(o + 0x11B) = Anim_FrameFlag(o) ? 1 : 0x11;
    Gig_Bridge(o);
    Orig_Call1(U32(GI_MAIN + U8(o + 0x54) * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    if (Bits_Check(b, 0xA)) U16(o + 0x40) = 0xF448;
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    s32 a = (s16)Angle_DiffTo(p + 0x40, o + 0xD8, U16(o + 0x4A));
    U32(o + 0x158) = iabs(a) < 0x500 && Bits_Check(b, 1) ? 2 : 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U16(pl + 0x1FA) > 0x80) Bits_Set(SHARED, 0, 0);
    Gig_Timer(o);
    if (U32(o + 0x1CC)) U32(o + 0x1CC)--;
    static const u16 tm[] = {0x1EC, 0x1EE, 0x1F6, 0x1F8};
    for (u32 i = 0; i < 4; i++) if (U16(o + tm[i])) U16(o + tm[i])--;
    if (U8(o + 0x18D)) U8(o + 0x18D)--;
    if (U32(o + 0x1D4)) U32(o + 0x1D4)--;
    Gig_Grab(o);
    U8(o + 0x1C8) = U8(o + 0x54); U8(o + 0x1CB) = U8(o + 0x57);
    U8(o + 0x1C9) = U8(o + 0x55); U8(o + 0x1CA) = U8(o + 0x56);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

/* state 0: kind, health, model, animations */
void DC2CALL Gig_Init(gptr o)                                       /* 0x433C70 */
{
    gptr b = GI_BITS(o);
    U32(o + 0x1C4) = 0;
    if (U8(G() + 0x1091) == 5 && U8(G() + 0x1090) == 3) U32(o + 0x1C4) = 1;
    if (U8(G() + 0x927)) U32(o + 0x1C4) = 2;
    if (U32(o + 0x1C4) == 2) Bits_Set(b, 0xD, 1);
    U32(o + 0x50) = 0xB;
    U8(o + 0x54)++;
    U16(o + 0x108) = 30000;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 30000;
    if (!U8(G() + 0x108E)) U16(o + 0x108) = 15000;
    U32(o + 0x12C) = 0x720C48;
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x128) = 0x720C78; U32(o + 0x134) = 0x720C88;
    stop(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Model_Setup(o, U32(o + 0x60) ? U32(o + 0x60) : 0x63F500);
    U32(o + 0x88) = 0x642D84;
    Anim_Start(o, 0, 0, 1, 5);
    Obj_Set118(o, 0, 0x4E80, 0x7590);
    DrawSync(0);
    Head_Reset(o);
    Obj_SetFeet(o, 0xE, 0xA);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x10; U16(o + 0x166) = 0x10; U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    Bits_Set(b, 1, 1);
    U32(o + 0x54) = 1;
    U32(0x74AF98) = 0;
    U8(o + 0xFE) = 3;
    U32(o + 0x184) |= 0x24;
    U8(o + 0x18D) = 0x5A;
    Flag_Set(2, 0x26, 1);
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* state 1: decides, then moves */
void DC2CALL Gig_Normal(gptr o)                                     /* 0x433E80 */
{
    Obj_SaveFeet(o);
    Orig_Call1(U32(GI_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(GI_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Gig_Hit(gptr o)                                        /* 0x433EB0 */
{
    Orig_Call1(U32(GI_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Gig_Die(gptr o)                                        /* 0x433ED0 */
{
    Orig_Call1(U32(GI_DIE + U8(o + 0x55) * 4), o);
}

/* its attacks hit the player: by the sub-state */
void DC2CALL Gig_Grab(gptr o)                                       /* 0x433EF0 */
{
    Enemy_AttackHit(o);
    if ((s8)U8(o + 0x119) >= 0) Orig_Call1(U32(GI_GRAB + U8(o + 0x55) * 4), o);
}

/* ---- helpers ---- */

/* the wait after the player was hurt or held; returns it */
u32 DC2CALL Gig_Timer(gptr o)                                       /* 0x436A90 */
{
    gptr pl = PL();
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    if (U8(pl + 0x54) == 2 || U8(pl + 0x54) == 5 || (U8(pl + 0x174) & 1)) {
        U32(o + 0x1D0) = 0x1E;
        if (!U8(G() + 0x108E)) U32(o + 0x1D0) = 0x3C;
    }
    return U32(o + 0x1D0);
}

/* an attack ends: collides again, the combo and the bite let go */
void DC2CALL Gig_Calm(gptr o)                                       /* 0x436B00 */
{
    gptr b = GI_BITS(o);
    U32(o + 0x50) |= 8;
    U32(o + 0x184) &= ~1u;
    Bits_Set(b, 5, 0); Bits_Set(b, 6, 0); Bits_Set(b, 7, 0); Bits_Set(b, 8, 0);
    if (Bits_Check(b, 0)) {
        Bits_Set(b, 0, 0);
        Bits_Set(SHARED, 0, 0);
    }
}

/* in a combo, at frame f of an attack: the next attack of it */
void DC2CALL Gig_Combo(gptr o, u32 f)                               /* 0x436B80 */
{
    gptr b = GI_BITS(o);
    if (!Bits_Check(b, 5)) return;
    if (Bits_Check(b, 6) && U16(o + 0x1F2) && U16(o + 0x92) == (u16)f && DIST(o) < 0x32C8) {
        switch ((s16)U16(o + 0x1F2)) {
        case 1: case 4: case 7: U32(o + 0x54) = 0x20D01; break;
        case 2:
            if (U16(o + 0x1F0)) { U32(o + 0x54) = 0x20C01; U16(o + 0x1F0) = 0; }
            else { U32(o + 0x54) = 0x20801; U16(o + 0x1F0) = 1; }
            break;
        case 3:
            if (Rand8() & 1) { U32(o + 0x54) = 0x20701; U16(o + 0x1F0) = 0; }
            else { U32(o + 0x54) = 0x20801; U16(o + 0x1F0) = 1; }
            break;
        case 5: U32(o + 0x54) = U16(o + 0x1F0) ? 0x20701 : 0x20C01; break;
        case 6:
            if (Rand8() & 1) { U32(o + 0x54) = 0x20801; U16(o + 0x1F0) = 0; }
            else { U32(o + 0x54) = 0x20C01; U16(o + 0x1F0) = 1; }
            break;
        case 8: case 9: U32(o + 0x54) = 0x20701; break;
        }
        U16(o + 0x1F2)--;
    }
    u16 c = U16(o + 0x1F2);
    if (Bits_Check(b, 7) && c && U16(o + 0x92) == (u16)f && DIST(o) < 0x32C8) {
        switch ((s16)c) {
        case 1: U32(o + 0x54) = 0x20D01; break;
        case 2: case 3: case 4: U32(o + 0x54) = 0x20701; break;
        }
        U16(o + 0x1F2) = c - 1;
    }
    c = U16(o + 0x1F2);
    if (Bits_Check(b, 8) && c && U16(o + 0x92) == (u16)f && DIST(o) < 0x32C8) {
        if ((s16)c == 1) U32(o + 0x54) = 0x20D01;
        else if ((s16)c == 2) U32(o + 0x54) = 0x20801;
        U16(o + 0x1F2) = c - 1;
    }
}

/* the bridge one (kind 1): the scene starts on event flag 6/0x8D, then it
 * walks the bridge, its sections break under its attacks; 1 when one broke */
u32 DC2CALL Gig_Bridge(gptr o)                                      /* 0x436D90 */
{
    gptr pl = PL(), b = GI_BITS(o);
    u32 res = 0;
    if (U32(o + 0x1C4) != 1) return 0;
    if (Flag_Check(6, 0x8D) && !Bits_Check(b, 9)) {
        Bits_Set(b, 9, 1);
        Bits_Set(b, 0xA, 1);
        Player_Release(o);
        Gig_Calm(o);
        stop(o);
        for (u32 n = 1; n <= 8; n++) Bits_Set(b, n, 0);
        Bits_Set(SHARED, 0x1F, 0);
        U32(o + 0x1CC) = (u32)((s32)S16(o + 0x10A) / 11);
        Anim_Start(o, 0x11, 0, 0x40, 0);
        U32(o + 0x54) = 0x601;
        U16(pl + 0x174) = 0;
        U32(pl + 0x50) &= 0xFFFFF6CFu;
        Anim_Start(pl, 0x16, 0, 1, 0);
        U32(pl + 0x54) = 1;
        U16(o + 0x40) = 0xF448; U16(o + 0x42) = 0; U16(o + 0x44) = 0xEE6C; U16(o + 0x4A) = 0;
        U16(pl + 0x40) = 0; U16(pl + 0x42) = 0xF060; U16(pl + 0x44) = 0xF448; U16(pl + 0x4A) = 0;
        U16(o + 0x1E4) = 0xF448; U16(o + 0x1E6) = 0; U16(o + 0x1E8) = 0x4E20;
    }
    if (S16(pl + 0x44) > S16(GI_BRIDGE)) Bits_Set(b, 0xE, 1);
    if (Bits_Check(b, 0xE) && Bits_Check(b, 0xA)) U8(o + 0x17B) = 0xA;
    if (!Bits_Check(b, 0xA)) return res;
    Zone_TrackPoint(o, o + 0x1E4);
    s16 k = 8;
    u32 i = 9;
    gptr p = GI_BRIDGE;
    do {
        if (!Flag_Check(7, i + 0xF) && (s8)Enemy_AttackInQuad(o, p - 2) >= 0) {
            if (S16(pl + 0x44) < S16(p)) bridge_catch(o, pl);
            if (k >= 0) for (u32 j = 0; j < i; j++) Flag_Set(7, 0x10 + j, 1);
            res = 1;
            Effect_Attach(o, 0x65B028);
        }
        k--; p -= 0x10; i--;
    } while (k > 1);
    return res;
}

/* the bridge one, while it walks the bridge: the end of it reached (event
 * 7/0x18), or the player near or behind; 1 when it chose */
u32 DC2CALL Gig_OnBridge(gptr o)                                    /* 0x4370B0 */
{
    gptr pl = PL(), b = GI_BITS(o);
    if (U32(o + 0x1C4) != 1) return 0;
    if (!Bits_Check(b, 0xA)) return 0;
    if (Flag_Check(7, 0x18)) {
        Bits_Set(b, 0xA, 0);
        Bits_Set(b, 0xD, 1);
        return 0;
    }
    if ((s32)S16(pl + 0x44) - S16(o + 0x44) < 0x1F4 || S16(o + 0x44) > 0xFA0) {
        Enemy_BiteOn(o);
        U32(o + 0x54) = 0xE01;
        if (S16(o + 0x44) > 0xBB8) {
            Bits_Set(b, 0xB, 1);
            stop(o);
        }
    }
    return 1;
}

/* the turn when the player is behind it or close (or one is down); 1 when
 * it chose. bite: may bite now */
u32 DC2CALL Gig_Turn(gptr o, u32 bite)                              /* 0x437180 */
{
    if (Bits_Check(GI_BITS(o), 0xA)) return 0;
    s32 a = ang_to(o);
    if (!U8(o + 0x13A) && !Bits_Check(SHARED, 0) && iabs(a) < 0x80 && (u8)bite && !U32(o + 0x1D0)) return 0;
    if (DIST(o) < 0x1B58 || Bits_Check(SHARED, 0x1F)) {
        U32(o + 0x54) = 0x301;
        return 1;
    }
    return 0;
}

/* walking: the player ahead and in reach; an attack by distance; 1 when it chose */
u32 DC2CALL Gig_Decide(gptr o, u32 ff)                              /* 0x434400 */
{
    gptr pl = PL(), b = GI_BITS(o);
    if (Gig_OnBridge(o)) return 0;
    s32 a = ang_to(o);
    if (U8(o + 0x13A) || Bits_Check(SHARED, 0) || iabs(a) >= 0x80) return 0;
    u8 f = (u8)ff;
    if (!U32(o + 0x1D0)) {
        u32 k = U32(o + 0x1C4);
        if (k == 1 || k == 2) {
            if (DIST(o) < 0x1F40 && S16(pl + 0x108) <= S16(pl + 0x10A) / 2 && !Flag_Check(2, 0x1A) && Rand8() < 0x20) {
                Enemy_BiteOn(o); U32(o + 0x54) = 0xB01; return 1;
            }
            if (DIST(o) < 0x2710 && !U16(o + 0x1EE) && Bits_Check(b, 0xD) && Rand8() < 0xC8) {
                Enemy_BiteOn(o); U32(o + 0x54) = 0x501; return 1;
            }
        }
        if (DIST(o) < 0x2AF8 && !f && !U16(o + 0x1EC) && Rand8() < 0x40) {
            Enemy_BiteOn(o); U32(o + 0x54) = 0x401; return 1;
        }
        if (DIST(o) < 0x2328 && DIST(o) > 0x1B58 && !f && Rand8() < 8) {
            Enemy_BiteOn(o); U32(o + 0x54) = 0xD01; return 1;
        }
        if (DIST(o) < 0x1B58) {
            if (Rand8() < 0x20) { Enemy_BiteOn(o); U32(o + 0x54) = 0x801; return 1; }
            if (DIST(o) < 0x1B58 && Rand8() < 0x80) { Enemy_BiteOn(o); U32(o + 0x54) = 0x701; return 1; }
        }
    }
    if ((U8(pl + 0x174) & 2) && DIST(o) > 0x2328 && DIST(o) < 0x2710 && !f && !Flag_Check(2, 0x1A)) {
        Enemy_BiteOn(o); U32(o + 0x54) = 0xA01; return 1;
    }
    return 0;
}

/* ---- deciding (state 1, before the move) ---- */

/* sub-state 0: walking */
void DC2CALL Gig_ThinkWalk(gptr o)                                  /* 0x433F20 */
{
    face(o);
    if (U8(o + 0x18D) || U8(o + 0x56) != 1) return;
    if (Gig_Decide(o, Anim_FrameFlag(o))) return;
    if (DIST(o) > 0x2EE0) { U32(o + 0x54) = 0x1000201; return; }
    Gig_Turn(o, 1);
}

/* sub-state 1: standing */
void DC2CALL Gig_ThinkStand(gptr o)                                 /* 0x4340C0 */
{
    if (U8(o + 0x56) != 1) return;
    Obj_TurnToPoint(o, (u16)(S16(o + 0x1E2) >> 1), o + 0xD8);
    if (!U16(o + 0x1F6)) { U32(o + 0x54) = 1; return; }
    if (DIST(o) > 0x2EE0) { U32(o + 0x54) = 0x201; return; }
    Gig_Turn(o, 0);
}

/* sub-state 2: running */
void DC2CALL Gig_ThinkRun(gptr o)                                   /* 0x4341D0 */
{
    if (Gig_OnBridge(o)) return;
    face(o);
    if (U8(o + 0x56) != 1) return;
    s32 a = ang_to(o);
    if (!U8(o + 0x13A) && !Bits_Check(SHARED, 0) && iabs(a) < 0x80 && !U32(o + 0x1D0)) {
        if (DIST(o) < 0x2328) { Enemy_BiteOn(o); U32(o + 0x54) = 0x901; return; }
    } else if (DIST(o) < 0x2328) { U32(o + 0x54) = 0x1000001; return; }
    Gig_Turn(o, 1);
}

/* sub-states 3..6 */
void DC2CALL Gig_ThinkFace(gptr o)                                  /* 0x434BD0 */
{
    face(o);
}

/* sub-states 7, 8, 12: turns until the bite comes (or in a combo) */
void DC2CALL Gig_ThinkBite(gptr o)                                  /* 0x434C70 */
{
    if (Bits_Check(GI_BITS(o), 5) || U16(o + 0x92) < 0xF) face(o);
}

/* sub-state 9 */
void DC2CALL Gig_ThinkSwipe(gptr o)                                 /* 0x435080 */
{
    if (U16(o + 0x92) < 0x14) face(o);
}

/* sub-state 10 */
void DC2CALL Gig_ThinkSnatch(gptr o)                                /* 0x4351B0 */
{
    if (U8(o + 0x56) == 1) face(o);
}

/* sub-state 11 */
void DC2CALL Gig_ThinkMaul(gptr o)                                  /* 0x4353C0 */
{
    if (U8(o + 0x56) == 1 && U16(o + 0x92) < 0x3C) face(o);
}

/* sub-state 13 */
void DC2CALL Gig_ThinkCharge(gptr o)                                /* 0x435780 */
{
    if (Bits_Check(GI_BITS(o), 5) || U16(o + 0x92) < 0x1E) face(o);
}

/* ---- moving (state 1) ---- */

/* sub-state 0: walks */
void DC2CALL Gig_Walk(gptr o)                                       /* 0x433F90 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(GI_BITS(o), 1, 1);
        if (!U8(o + 0x57)) Anim_Start(o, 0, Anim_FrameFlag(o) ? 0 : 0xB, 1, 0x14);
        else {
            s32 x = ((s32)U16(o + 0x92) + 0xE) % 64;
            Anim_Start(o, 0, (u32)((x * 94 / 64 + 3) % 94 / 4), 1, 0x14);
        }
        break;
    case 1:
        Anim_Step(o);
        Value_Approach(o + 0xF4, 0x14, 0x78);
        break;
    }
    if (U16(o + 0x92) == 0x5B) step(o, 0x30);
    if (U16(o + 0x92) == 0x2B) step(o, 0x31);
}

/* sub-state 1: stands a while */
void DC2CALL Gig_Stand(gptr o)                                      /* 0x434120 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        stop(o);
        Bits_Set(GI_BITS(o), 1, 1);
        u32 ff = Anim_FrameFlag(o);
        Anim_Start(o, ff ? 0xA : 9, 0, 1, 0x14);
        U16(o + 0x1F6) = (u16)((u8)Rand8() % 10 + 0x14);
        break;
    }
    case 1:
        Anim_Step(o);
        break;
    }
}

/* sub-state 2: runs */
void DC2CALL Gig_Run(gptr o)                                        /* 0x4342A0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(GI_BITS(o), 1, 1);
        if (!U8(o + 0x57)) Anim_Start(o, 2, Anim_FrameFlag(o) ? 4 : 0xC, 1, 0x14);
        else {
            s32 x = (s32)U16(o + 0x92) << 6;
            Anim_Start(o, 2, (u32)((x / 94 + 0x35) % 64 / 4), 1, 0x14);
        }
        break;
    case 1:
        Anim_Step(o);
        if (Bits_Check(GI_BITS(o), 0xA)) Value_Approach(o + 0xF4, 0x32, 0x15E);
        else Value_Approach(o + 0xF4, 0x14, 0xD2);
        break;
    }
    if (U16(o + 0x92) == 0x10) step(o, 0x30);
    if (U16(o + 0x92) == 0x30) step(o, 0x31);
}

/* sub-state 3: turns from standing, then walks */
void DC2CALL Gig_TurnStep(gptr o)                                   /* 0x434670 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        stop(o);
        u32 ff = Anim_FrameFlag(o);
        Anim_Start(o, ff ? 6 : 5, 2, 0x48, 0x14);
        Bits_Set(GI_BITS(o), 1, 1);
        break;
    }
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) > 0x1B) U32(o + 0x54) = 0x101;
        break;
    }
    if (U16(o + 0x92) == 0x11) quake(o);
}

/* sub-state 4: the roar; the player stunned through G+0x92C 0x434900 */
void DC2CALL Gig_Roar(gptr o)                                       /* 0x434730 */
{
    gptr pl = PL(), b = GI_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 1, 0, 0x40, 0x14);
        Bits_Set(b, 1, 0); Bits_Set(b, 3, 1); Bits_Set(b, 4, 0);
        U16(o + 0x1EC) = 0x258;
        U16(o + 0x1F4) = 0xA;
        Flash_Play(o, 0x65B294);
        return;
    case 1:
        break;
    default:
        return;
    }
    if (Anim_Step(o)) {
        Gig_Calm(o);
        U32(o + 0x54) = 0x101;
        Bits_Set(b, 3, 0);
        U16(o + 0x1EE) = 0x12C;
    }
    if (U16(o + 0x92) == 0x2C) {
        U32(o + 0x50) &= ~8u;
        Shake_Set(4, 0, 0x2E);
        if (U8(pl + 0x54) != 2 && U8(pl + 0x54) != 5 && !(U8(pl + 0x174) & 1) && !Flag_Check(2, 0x1A)) {
            Player_Hold(o, 0, 0x48E100);
            Anim_Start(pl, U32(U32(o + 0x88) + 0x70), 0, 0x40, 0);    /* +0x70 + 4 * Stub_Return0() */
            U32(G() + 0x92C) = 0x434900;
        }
        U8(G() + 0xB58) = 0x80; U8(G() + 0xB59) = 0x80; U8(G() + 0xB5A) = 0x80;
        U8(G() + 0xB5B) = 0; U8(G() + 0xB5C) = 0;
    }
    if (U16(o + 0x92) >= 0x2C) DrawSync(0);
    u16 f = U16(o + 0x92);
    Bits_Set(b, 4, f >= 0x2C && f < 0x4D);
}

/* the player's state while stunned by the roar (G+0x92C), pl the player */
void DC2CALL Gig_RoarStun(gptr pl)                                  /* 0x434900 */
{
    gptr o = U32(pl + 0x1D8), b = GI_BITS(o);
    Obj_TurnToPoint(pl, (u16)(U16(o + 0x1E2) << 4), o + 0x40);
    switch (U8(pl + 0x57)) {
    case 0: {
        Anim_Step(pl);
        if (!Bits_Check(b, 3)) {
            U8(pl + 0x57)++;
            Anim_Start(pl, U32(U32(o + 0x88) + 0x74), 0, 0x40, 0);   /* +0x74 + 4 * Stub_Return0() */
            return;
        }
        if (!Bits_Check(b, 4)) return;
        gptr v = Tmp_Push(8);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0xA; U16(v + 6) = 0;
        Vec_RotY(v, v, U16(o + 0x4A));
        U16(pl + 0x40) += U16(v); U16(pl + 0x44) += U16(v + 4);
        Tmp_Pop(8);
        Obj_CollideWalls(pl);
        if ((U8(0x87DC08) & 8) && (U8(G() + 0x10B4) & 0xF)) U16(o + 0x1F4) = 0xA;
        else U16(o + 0x1F4)--;
        if (U16(o + 0x1F4) || !U8(G() + 0x108E)) return;
        Player_Hurt(o, 0, 3, kind_byte(o));
        break;
    }
    case 1:
        if (Anim_Step(pl)) {
            Anim_Set(pl, 0x16, 0, 1, 0xA);
            Player_AnimReset();
        }
        break;
    }
}

/* sub-state 5: the threat; it may start a combo */
void DC2CALL Gig_Threat(gptr o)                                     /* 0x434AA0 */
{
    gptr b = GI_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 3, 0, 0x40, 0xA);
        Bits_Set(b, 1, 0);
        U16(o + 0x1EE) = 0x12C;
        break;
    case 1: {
        if (!Anim_Step(o)) break;
        s16 a = (s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A));
        if (DIST(o) < 0x2710 && iabs(a) < 0x80) {
            Enemy_BiteOn(o);
            U32(o + 0x54) = 0xC01;
            Bits_Set(b, 5, 1); Bits_Set(b, 6, 1);
            U16(o + 0x1F2) = 9;
            U32(o + 0x1D4) = 0;
            break;
        }
        U8(o + 0x56)++;
        Anim_Start(o, 4, 0, 0x40, 0xA);
        break;
    }
    case 2:
        if (!Anim_Step(o)) break;
        Gig_Calm(o);
        U32(o + 0x54) = 1;
        break;
    }
}

/* sub-state 6: shakes its head */
void DC2CALL Gig_Shake(gptr o)                                      /* 0x434BF0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 0x11, 4, 0x40, 0x10);
        Bits_Set(GI_BITS(o), 1, 0);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) > 0x37) U32(o + 0x54) = 1;
        break;
    }
}

/* an attack's animation ended: the combo goes on or it turns, walks */
static void attack_end(gptr o)
{
    gptr b = GI_BITS(o);
    if (Bits_Check(b, 5)) {
        Bits_Set(b, 5, 0);
        U16(o + 0x1F2) = 0;
        if (Bits_Check(b, 6)) { Bits_Set(b, 6, 0); U32(o + 0x54) = 0x401; return; }
        u32 n = Bits_Check(b, 7) ? 7 : Bits_Check(b, 8) ? 8 : 0;
        if (n) {
            Bits_Set(b, n, 0);
            U32(o + 0x54) = 0x301;
            Gig_Calm(o);
            return;
        }
    }
    Gig_Calm(o);
    next_move(o, 1);
}

/* a combo attack: anim from frame 0 (case 0) or fr (case 2, the next of a
 * combo); at frame cf the combo goes on */
static u32 attack(gptr o, u32 anim, u32 bl0, u32 fr, u32 bl2, u32 cf)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, anim, 0, 0x40, bl0);
        Bits_Set(GI_BITS(o), 1, 0);
        return 1;
    case 1:
        if (Anim_Step(o)) { attack_end(o); return 0; }
        Gig_Combo(o, cf);
        if (Bits_Check(GI_BITS(o), 5)) U16(o + 0xF4) = U16(o + 0xD4);
        return 2;
    case 2:
        U8(o + 0x56)--;
        stop(o);
        Anim_Start(o, anim, fr, 0x40, bl2);
        Bits_Set(GI_BITS(o), 1, 0);
        return 3;
    }
    return 4;
}

/* sub-state 7: a bite; kinds 1/2 may start a combo */
void DC2CALL Gig_Bite(gptr o)                                       /* 0x434CB0 */
{
    gptr b = GI_BITS(o);
    u32 r = attack(o, 0xB, 0x14, 6, 0x14, 0x3C);
    if (r == 1) {
        u32 k = U32(o + 0x1C4);
        if (k == 0 || k > 2 || Bits_Check(b, 5)) return;
        if (Rand8() < 0x80) {
            Bits_Set(b, 5, 1); Bits_Set(b, 7, 1);
            U16(o + 0x1F2) = 4; U32(o + 0x1D4) = 0;
        }
        if (Rand8() < 0x80) {
            Bits_Set(b, 5, 1); Bits_Set(b, 8, 1);
            U16(o + 0x1F2) = 2; U32(o + 0x1D4) = 0;
        }
    } else if (r == 2 && U16(o + 0x92) == 0x2C) Shake_Set(4, 0, 0x1E);
}

/* sub-state 8: the other bite */
void DC2CALL Gig_Bite2(gptr o)                                      /* 0x434EF0 */
{
    attack(o, 0xC, 0x14, 7, 0x14, 0x46);
}

/* sub-state 12: a lunge */
void DC2CALL Gig_Lunge(gptr o)                                      /* 0x4355D0 */
{
    if (attack(o, 0xF, 0xA, 3, 0x1E, 0x3C) && U16(o + 0x92) == 0x1B) quake(o);
}

/* sub-state 13: a charge */
void DC2CALL Gig_Charge(gptr o)                                     /* 0x4357C0 */
{
    if (attack(o, 0x10, 0x1E, 0xA, 0x14, 0x50) && U16(o + 0x92) == 0x43) quake(o);
}

/* sub-state 9: the tail swipe (o+0x57 which half of the walk it came from) */
void DC2CALL Gig_Swipe(gptr o)                                      /* 0x4350B0 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        u16 f = U16(o + 0x92);
        U8(o + 0x57) = f > 0x1C && f < 0x3C ? 1 : 0;
        stop(o);
        Anim_Start(o, U8(o + 0x57) ? 8 : 7, 0, 0x40, 0xA);
        Bits_Set(GI_BITS(o), 1, 0);
        break;
    }
    case 1:
        if (!Anim_Step(o)) break;
        Gig_Calm(o);
        next_move(o, 1);
        break;
    }
    if (U16(o + 0x92) == 0x10 || U16(o + 0x92) == 0x26) quake(o);
}

/* sub-state 10: snatches the player up and kills it */
void DC2CALL Gig_Snatch(gptr o)                                     /* 0x4351E0 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 0x11, 0, 0x40, 0x19);
        Bits_Set(GI_BITS(o), 1, 0);
        break;
    case 1:
        if (!Anim_Step(o)) break;
        if (U8(pl + 0x174) & 2) {
            U8(o + 0x56)++;
            U32(o + 0x50) &= ~8u;
            Anim_Start(o, 0x12, 0, 0x40, 0xA);
            Flash_Play(o, 0x65B314);
            Effect_Attach(o, 0x659238);
            Player_Hold(o, 0, 0x48E100);
            U32(pl + 0x50) |= 0x30;
            U32(o + 0x50) |= 0x30;
            break;
        }
        U8(o + 0x56) = 3;
        Gig_Calm(o);
        Anim_Start(o, 0x13, 0, 0x40, 0xA);
        break;
    case 2:
        Anim_Step(o);
        if (U16(o + 0x92) == 8) {
            Anim_Start(pl, U32(U32(o + 0x88) + 0x84), 0, 0x40, 0);
            Flag_Set(1, 1, 1);
            place_player(o, pl);
        }
        if (U16(o + 0x92) == 0x50) GameOver_Start(0);
        if (U16(o + 0x92) == 0x10) Shake_Set(5, 0, 0x32);
        break;
    case 3:
        if (!Anim_Step(o)) break;
        Gig_Calm(o);
        next_move(o, 1);
        break;
    }
}

/* sub-state 11: mauls (case 2 from the hit: the player held and killed) */
void DC2CALL Gig_Maul(gptr o)                                       /* 0x4353F0 */
{
    gptr pl = PL(), b = GI_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 0xD, 0, 0x40, 0xA);
        Bits_Set(b, 1, 0);
        break;
    case 1:
        if (U16(o + 0x92) == 0x60) Bits_Set(b, 2, 1);
        if (U16(o + 0x92) == 0x69) Bits_Set(b, 2, 0);
        if (Anim_Step(o)) {
            Gig_Calm(o);
            next_move(o, 1);
        }
        if (U16(o + 0x92) == 0x70) Shake_Set(3, 0, 0x1E);
        break;
    case 2:
        U8(o + 0x56)++;
        Anim_Step(o);
        if (U16(o + 0x92) == 0x70) Shake_Set(5, 0, 0x32);
        Anim_Start(pl, U32(U32(o + 0x88) + 0x80), 0, 0x40, 0);
        U32(pl + 0x50) |= 0x30;
        U32(o + 0x50) |= 0x30;
        Flag_Set(1, 1, 1);
        place_player(o, pl);
        break;
    case 3:
        place_player(o, pl);
        Anim_Step(o);
        if (U16(o + 0x92) == 0x14) GameOver_Start(0);
        break;
    }
}

/* sub-state 14: smashes the bridge (the player on it falls) */
void DC2CALL Gig_Smash(gptr o)                                      /* 0x435970 */
{
    gptr pl = PL(), b = GI_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 0x14, 2, 0x40, 0xA);
        Bits_Set(b, 1, 0);
        return;
    case 1:
        break;
    default:
        return;
    }
    Anim_Step(o);
    if (U16(o + 0x92) > 0x3C) { Gig_Calm(o); U32(o + 0x54) = 1; }
    if (U16(o + 0x92) == 0x28 && Bits_Check(b, 0xB)) {
        if (S16(pl + 0x44) < S16(GI_BRIDGE)) bridge_catch(o, pl);
        for (u32 i = 0x18; i > 0x11; i--) Flag_Set(7, i, 1);
        Effect_Attach(o, 0x65B028);
    }
    Bits_Set(b, 0xC, U16(o + 0x92) <= 0x28);
    if (U16(o + 0x92) == 0x25) {
        Shake_Set(5, 0, 0x32);
        Flash_Play(o, 0x65B330);
    }
}

/* the player's state while it falls with the bridge (G+0x92C), pl the player */
void DC2CALL Gig_BridgeFall(gptr pl)                                /* 0x435B00 */
{
    gptr o = U32(pl + 0x1D8);
    Anim_Step(pl);
    if (U16(pl + 0x92) == 0x46) GameOver_Start(0);
    if (U16(G() + 0x664) == 4) return;
    U16(o + 0x1EA) += 0x14;
    U16(pl + 0x42) += U16(o + 0x1EA);
    U16(pl + 0x44) += 0x82;
    U16(pl + 0x4C) += 0x10;
}

/* ---- the attacks hit the player ---- */

/* from ahead (|angle| < 0x400) knocked back (light in a combo), else down;
 * the light back damage is what the original computes: (-1 & ~0xFF | m) + d */
static void hit_player(gptr o, s16 a, u32 fc, u32 f, u32 bc, u32 bk)
{
    if (iabs(a) < 0x400) {
        if (Bits_Check(GI_BITS(o), 5)) Player_Hurt(o, fc, 1, 0);
        else Player_Hurt(o, f, 2, kind_byte(o));
    } else Player_Hurt(o, Bits_Check(GI_BITS(o), 5) ? bc : bk, 1, 0);
}

/* sub-state 7 */
void DC2CALL Gig_BiteHit(gptr o)                                    /* 0x435B60 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x65B2B8);
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x7D0; U16(v + 6) = 0;
    gptr p = Obj_Part(U32(o + 0x64), 3);
    ApplyMatrixSV(p + 0x20, v, v);
    U16(v) += U16(p + 0x40); U16(v + 2) += U16(p + 0x42); U16(v + 4) += U16(p + 0x44);
    u16 fl = (u16)Floor_Height(v, v, 0x80, 0);
    Tmp_Pop(8);
    if (fl) Effect_Attach(o, 0x6586C0);
    hit_player(o, a, 0x96, 0x12C, 0x53, 0xFA);
}

/* sub-state 8 */
void DC2CALL Gig_Bite2Hit(gptr o)                                   /* 0x435CC0 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x65B2C8);
    Effect_Attach(o, 0x65891C);
    hit_player(o, a, 0x96, 0x12C, 0x53, 0xFA);
}

/* sub-state 9 */
void DC2CALL Gig_SwipeHit(gptr o)                                   /* 0x435DB0 */
{
    static const u32 fx[2] = {0x65A264, 0x65A3D0};
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x65B2A8);
    Effect_Attach(o, fx[U8(o + 0x57)]);
    if (iabs(a) < 0x400) Player_Hurt(o, 0x12C, 2, kind_byte(o));
    else Player_Hurt(o, 0xFA, 1, 0);
}

/* sub-state 11: the maul caught the player */
void DC2CALL Gig_MaulHit(gptr o)                                    /* 0x435E90 */
{
    if (U8(o + 0x56) != 1) return;
    U8(o + 0x56)++;
    Anim_Start(o, 0xE, 0, 0x40, 0xA);
    U32(o + 0x50) &= ~8u;
    Flash_Play(o, 0x65B2D8);
    Effect_Attach(o, 0x658AD8);
    Player_Hold(o, 0, 0x48E100);
}

/* sub-state 12 */
void DC2CALL Gig_LungeHit(gptr o)                                   /* 0x435EF0 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x65B2EC);
    Effect_Attach(o, 0x65AEBC);
    hit_player(o, a, 0xC8, 0x190, 0x64, 0x12C);
}

/* sub-state 13 */
void DC2CALL Gig_ChargeHit(gptr o)                                  /* 0x435FE0 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x65B300);
    Effect_Attach(o, 0x659004);
    hit_player(o, a, 0xC8, 0x190, 0x64, 0x12C);
}

/* sub-state 14 */
void DC2CALL Gig_SmashHit(gptr o)                                   /* 0x4360D0 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    if (iabs(a) < 0x400) Player_Hurt(o, 0x190, 2, 0x64);
    else Player_Hurt(o, 0x12C, 1, 0);
}

/* ---- hit (state 2) ---- */

/* sub-state 0: the damage (o+0x176), its reaction */
void DC2CALL Gig_Damage(gptr o)                                     /* 0x436140 */
{
    static const u32 fx[6] = {0x65A71C, 0x65A798, 0x65A814, 0x65A8E0, 0x65A9AC, 0x65AA78};
    gptr b = GI_BITS(o);
    s32 hp0 = S16(o + 0x108);
    if (U8(o + 0x184) & 8) U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1));
    if (!U32(o + 0x1C4) && U8(o + 0x17D) != 0x19) U16(o + 0x176) = U16(o + 0x176) / 3;
    prev_state(o);
    if (Bits_Check(b, 0xC)) {
        Bits_Set(b, 0xC, 0);
        U32(o + 0x1CC) = 0;
        U32(o + 0x54) = Anim_FrameFlag(o) << 24 | 0x102;
    }
    if (Bits_Check(b, 2)) {
        if (U16(o + 0x172) == 3) U32(o + 0x54) = 0x1000202;
        else {
            U32(o + 0x1CC) += U16(o + 0x176);
            if ((s32)U32(o + 0x1CC) > S16(o + 0x10A) / 30) {
                U32(o + 0x1CC) = 0;
                U32(o + 0x54) = 0x202;
            }
        }
    } else if (!U16(o + 0x1F8)) {
        U32(o + 0x1CC) += U16(o + 0x172) == 3 ? U16(o + 0x176) : U16(o + 0x176) >> 1;
        if ((s32)U32(o + 0x1CC) > S16(o + 0x10A) / 11) {
            U32(o + 0x1CC) = 0;
            U32(o + 0x54) = Anim_FrameFlag(o) << 24 | 0x102;
        }
    }
    if (Bits_Check(b, 5)) {
        U32(o + 0x1D4) += U16(o + 0x176);
        if ((s32)U32(o + 0x1D4) > S16(o + 0x10A) / 4) U32(o + 0x1D4) = 0;
        else prev_state(o);
    }
    if (!U32(o + 0x1C4)) {
        Bits_Set(SHARED, 0x1E, 0);
        if (U8(o + 0x17D) == 0x19) {
            if (Bits_Check(SHARED, 0x1F)) {
                U32(o + 0x54) = 0x302;
                Bits_Set(SHARED, 0x1E, 1);
                if (Anim_FrameFlag(o)) Bits_Set(SHARED, 0x1D, 1);
            } else {
                U16(o + 0x176) = 0;
                prev_state(o);
            }
        }
    }
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        Gig_Calm(o);
        stop(o);
        for (u32 n = 1; n <= 8; n++) Bits_Set(b, n, 0);
        Bits_Set(SHARED, 0x1F, 0);
        Se_StopGroup(0x38); Se_StopGroup(0x39); Se_StopGroup(0x3C);
        U16(o + 0x1F8) = 0x3C;
        if (U8(o + 0x184) & 8) Score_Event(o);
        u32 k = U32(o + 0x1C4);
        U16(o + 0x108) -= U16(o + 0x176);
        U8(o + 0x17B) = 0xA;
        switch (k) {
        case 0:
            if (!Bits_Check(SHARED, 0x1E)) break;
            if (S16(o + 0x108) <= 0) {
                U32(o + 0x50) &= ~8u;
                U16(o + 0x108) = 0xFFFF;
                U32(o + 0x54) = (u32)U8(o + 0x55) << 8 | 3;
                Score_Kill(o);
                Flag_Set(4, U8(o + 0x11C), 1);
                break;
            }
            Score_Hit(o, (u32)(hp0 - S16(o + 0x108)) / 5);
            break;
        case 1:
            if (S16(o + 0x108) <= 0) U16(o + 0x108) = 1;
            Score_Hit(o, (u32)(hp0 - S16(o + 0x108)) / 5);
            break;
        case 2:
            if (S16(o + 0x108) < 0) {
                U32(o + 0x50) &= ~8u;
                U16(o + 0x108) = 0xFFFF;
                U32(o + 0x54) = (u32)U8(o + 0x55) << 8 | 3;
                Score_Kill(o);
                break;
            }
            Score_Hit(o, (u32)(hp0 - S16(o + 0x108)) / 5);
            break;
        }
    } else {
        U8(o + 0x17B) = 0xA;
        U16(o + 0x108) -= (u16)(U16(o + 0x176) / 5);
        if (!(U8(o + 0x170) & 2)) {
            s16 d = (s16)Angle_Diff((U16(o + 0x178) - 0x800) & 0xFFF, U16(o + 0x4A));
            Effect_Attach(o, fx[(d > 0 ? 0 : 3) + (u8)Rand8() % 3]);
        }
        hit_side_prop(o);
        if (S16(o + 0x108) <= 0) U16(o + 0x108) = 1;
    }
    Orig_Call1(U32(GI_MAIN + U8(o + 0x54) * 4), o);
}

/* sub-state 1: flinches (o+0x57 the side) */
void DC2CALL Gig_Flinch(gptr o)                                     /* 0x436740 */
{
    static const u32 anim[2] = {0x17, 0x18}, fx[2] = {0x659804, 0x659A60};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, anim[U8(o + 0x57)], 0, 0x40, 2);
        Effect_Attach(o, fx[Anim_FrameFlag(o)]);
        hit_side_prop(o);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) > 0x14) next_move(o, 0x101);
        break;
    }
}

/* sub-state 2: staggers */
void DC2CALL Gig_Stagger(gptr o)                                    /* 0x436860 */
{
    static const u32 anim[2] = {0x19, 0x1A}, fx[2] = {0x65A264, 0x65A3D0};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, anim[U8(o + 0x57)], 0, 0x40, 2);
        Effect_Attach(o, fx[Anim_FrameFlag(o)]);
        break;
    case 1:
        if (Anim_Step(o)) next_move(o, 0x101);
        break;
    }
}

/* sub-state 3: knocked down (the kind-0 one by the other) */
void DC2CALL Gig_Down(gptr o)                                       /* 0x436910 */
{
    static const u32 anim[2] = {0x15, 0x16};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U8(o + 0x17B) = 0x96;
        Anim_Start(o, anim[Anim_FrameFlag(o)], 0, 0x40, 2);
        break;
    case 1:
        if (!Anim_Step(o)) break;
        next_move(o, 1);
        Bits_Set(SHARED, 0x1E, 0);
        break;
    }
}

/* ---- die (state 3) ---- */

void DC2CALL Gig_Dead(gptr o)                                       /* 0x4369B0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x1B, 0, 0x40, 2);
        U32(o + 0x50) &= ~8u;
        stop(o);
        Flash_Play(o, 0x65B344);
        Bits_Set(GI_BITS(o), 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U32(o + 0x50) |= 0x30;
            Layers_Assign();
        }
        if (U16(o + 0x92) == 0x85) {
            Shake_Set(5, 0, 0x32);
            if (U16(o + 0x92) == 0x85) Prop_SpawnC(1, o + 0x40, 2, 0x78, 1, 0);
        }
        break;
    }
}
