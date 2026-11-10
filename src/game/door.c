/* The door transition (the room change): AOT_Door stores the destination
 * (0x7593B4 -> &aot+0x14: x, y, z, yaw, room, area, door type, light) and
 * starts Door_Task. The task keeps its state in D = 0x748A60 (0x24A0 bytes):
 * +0x2474 mode (0x71FFC4: set-up, run, end), +0x2475 step (in mode run:
 * 0x71FFD0 area switch, room load, music), +0x2476 sub-step, +0x2478/79
 * destination room/area, +0x247A the room left, +0x247D done bits (1 music,
 * 0x10 door animation), +0x247E the door animation ended, +0x2480 door type
 * (0x71F1F0, 0x20 bytes each: +0 player animation table, +4/+8 the two door
 * models, +0xC per-frame handler, +0x10 second model's x, +0x12/13 their
 * yaws >> 4, +0x14 camera 0x71F020), +0x2481 light (0x71FD70), +0x2491,
 * +0x2492 the fade-in runs, +0x2494/98 the room name's width/text,
 * +0x249C the player's animation table; objects at +0x530 (0x260 each: the
 * player stand-in, then the door models), their parts at +0xC50. */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"
#include "dc2/task.h"
#include "dc2/pcio.h"

#define DOOR       0x748A60u
#define A_DOOR_TYPES  0x71F1F0u
#define A_AREAS    0x73825Cu

/* the destination of the door being used */
gptr DC2CALL Door_Dest(void)                                       /* 0x48B820 */
{
    return U32(0x7593B4);
}

/* the room's name (area, room as signed bytes) */
gptr DC2CALL Room_Name(u32 area, u32 room)                         /* 0x495C90 */
{
    return U32(U32(A_AREAS + (s32)(s8)area * 0x18) + (s32)(s8)room * 0x18 + 0x10);
}

/* the scene's light list emptied (G+0xA3C -> 0x74B5D0) */
void DC2CALL Lights_ListReset(void)                                /* 0x46B890 */
{
    for (u32 k = 0; k < 5; k++) U32(0x74B5D0 + 4 * k) = 0;
    U32(G() + 0xA3C) = 0x74B5D0;
}

/* camera from eye to target: angles into ang, distance into dist+4 */
void DC2CALL Camera_Aim(gptr eye, gptr target, gptr ang, gptr dist)   /* 0x4038E0 */
{
    U16(dist + 2) = 0; U16(dist) = 0;
    U16(dist + 4) = (u16)Camera_Distance(eye, target);
    Angles_LookAt(eye, target, ang);
}

/* the player's stand-in for the door scene (object idx of D at pos): the
 * player's model and animations, alone in the object list */
void DC2CALL Door_SetupPlayer(gptr d, u32 idx, gptr pos)           /* 0x41E6B0 */
{
    gptr pl = U32(G() + 0x940), model = U32(pl + 0x60);
    U32(d + 0x249C) = U32(pl + 0x88);
    gptr o = d + 0x530 + (u8)idx * 0x260;
    U32(G() + 0x940) = o;
    U32(G() + 0x938) = d + 0xC50;
    U32(0x87DC0C) = 0x1E;
    U32(o + 0x50) = 3;
    U16(o + 0x40) = U16(pos); U16(o + 0x42) = U16(pos + 2); U16(o + 0x44) = U16(pos + 4);
    U16(o + 0x4C) = 0; U16(o + 0x4A) = 0; U16(o + 0x48) = 0;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U16(o + 0x5A) = 0;
    U32(o + 0x88) = U32(d + 0x249C);
    U32(0x87F940) = 1;
    Model_Setup(o, model);
}

/* a door model after the stand-in (object idx + 1) at x, turned yaw */
void DC2CALL Door_SetupModel(gptr d, u32 idx, gptr model, u32 x, u32 yaw)   /* 0x41E790 */
{
    (void)d;
    gptr o = U32(G() + 0x940) + ((u8)idx + 1) * 0x260;
    U16(o + 0x40) = (u16)x;
    U32(o + 0x50) = 3;
    U16(o + 0x4C) = 0; U16(o + 0x48) = 0;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U16(o + 0x5A) = 0; U16(o + 0x44) = 0; U16(o + 0x42) = 0;
    U16(o + 0x4A) = (u16)yaw;
    U32(0x87F940)++;
    Model_Setup(o, model);
}

/* the door scene's camera (eye, target, then the stand-in's position) */
void DC2CALL Door_SetCamera(gptr cam)                              /* 0x41E820 */
{
    gptr pl = U32(G() + 0x940), c = G() + 0x778;
    memset(MEM(c), 0, 0x23 * 4);
    U16(c + 0x28) = U16(cam); U16(c + 0x2A) = U16(cam + 2); U16(c + 0x2C) = U16(cam + 4);
    U16(c + 0x38) = U16(cam + 6); U16(c + 0x3A) = U16(cam + 8); U16(c + 0x3C) = U16(cam + 0xA);
    Camera_Aim(c + 0x28, c + 0x38, c + 0x20, c + 0x30);
    gptr v = Tmp_Push(8);
    U16(v) = (u16)-U16(c + 0x20); U16(v + 2) = (u16)-U16(c + 0x22); U16(v + 4) = (u16)-U16(c + 0x24);
    RotMatrix(v, c);
    Tmp_Pop(8);
    SetGeomScreen(0x140);
    U16(pl + 0x40) = U16(cam + 0xC); U16(pl + 0x42) = U16(cam + 0xE); U16(pl + 0x44) = U16(cam + 0x10);
}

/* the door scene's light (0x71FD70 by D+0x2481): back colour and one light */
void DC2CALL Door_SetLight(gptr d)                                 /* 0x41E8F0 */
{
    gptr li = 0x71FD70 + U8(d + 0x2481) * 16, s = d + 0x244C;
    U8(d + 0x2438) = U8(li); U8(d + 0x2439) = U8(li + 1); U8(d + 0x243A) = U8(li + 2);
    U8(d + 0x243B) = 1;
    SetBackColor(U8(d + 0x2438), U8(d + 0x2439), U8(d + 0x243A));
    U32(d + 0x2440) = 0; U32(d + 0x2444) = 0; U32(d + 0x2448) = 0;
    U8(s + 3) = 1;
    U8(s) = U8(li + 3); U8(s + 1) = U8(li + 4); U8(s + 2) = U8(li + 5);
    U16(s + 4) = U16(li + 6); U16(s + 6) = U16(li + 8); U16(s + 8) = U16(li + 0xA);
    U16(s + 0xA) = U16(li + 0xC); U16(s + 0xE) = U16(li + 0xE);
    memcpy(MEM(G() + 0xC), MEM(s), 0x14);
    U8(G() + 0x7C4) = 0;
    Lights_ListReset();
}

/* mode 0: fade out, wait, the door's file and the player's state, then the
 * door scene (stand-in, models, camera, light) */
void DC2CALL Door_Init(gptr d)                                     /* 0x41E310 */
{
    switch (U8(d + 0x2475)) {
    case 0:
        Fade_Set(1, 0xA, 1);
        U8(d + 0x2475)++;
        /* fall through */
    case 1:
        if (Flag_Check(2, 6)) return;
        U32(G() + 0x93C) = d;
        U8(d + 0x2475)++;
        /* fall through */
    case 2: {
        if (File_IsBusy()) return;
        gptr dst = Door_Dest();
        U8(d + 0x2480) = U8(dst + 0xA); U8(d + 0x2481) = U8(dst + 0xB);
        U8(d + 0x2475)++;
        Room_Reset();
        u16 f = U16(0x71FEB0 + U8(d + 0x2480) * 2);
        if (f) File_RequestLoad(f);
        gptr pl = U32(G() + 0x940);
        if (U8(G() + 0x108F) == 1) U16(G() + 0x11FC) = U16(pl + 0x108);
        else U16(G() + 0x11F8) = U16(pl + 0x108);
        U16(G() + 0x1200) = U16(pl + 0x17E);
        Snd_LeaveRoom();
    }
        /* fall through */
    case 3:
        if (File_IsBusy()) return;
        U8(d + 0x2475)++;
        /* fall through */
    case 4:
        break;
    default:
        return;
    }
    U8(d + 0x2475)++;
    gptr dst = Door_Dest();
    U16(G() + 0x840) = U16(dst); U16(G() + 0x842) = U16(dst + 2);
    U16(G() + 0x844) = U16(dst + 4); U16(G() + 0x846) = U16(dst + 6);
    U16(d + 0x2478) = U16(dst + 8);
    u8 t = U8(d + 0x2480);
    gptr e = A_DOOR_TYPES + t * 0x20;
    U16(d + 0x247A) = U16(G() + 0x1090);
    if (t == 0x44) U32(0x87DE00) = 3;
    gptr v = Tmp_Push(8);
    U32(v) = 0; U32(v + 4) = 0;
    Door_SetupPlayer(d, 0, v);
    Tmp_Pop(8);
    gptr o = U32(G() + 0x940);
    Anim_Start(o, 0x16, 0, 0x11, 0);
    Anim_Step(o);
    U8(o + 0x118) = 0;
    U32(o + 0x88) = U32(e);
    U8(o + 0xFE) = 3;
    if (U32(e + 4)) Door_SetupModel(d, 0, U32(e + 4), 0, (u32)U8(e + 0x12) << 4);
    if (U32(e + 8)) Door_SetupModel(d, 1, U32(e + 8), U16(e + 0x10), (u32)U8(e + 0x13) << 4);
    Door_SetCamera(0x71F020 + U8(e + 0x14) * 0x14);
    Door_SetLight(d);
    U8(d + 0x2474)++;
    U8(d + 0x2475) = 0;
    U8(d + 0x2491) = 5;
    if (Flag_Check(2, 0x14) && U32(G() + 0xC14)) U8(d + 0x2492) = 1;
    U8(G() + 0xB4C) = 0xFF;
    Shake_Set(0xFFFFFFFF, 0, 0);
    Flag_Set(3, 0, 0);
    Flag_Set(2, 0x12, 0);
    Flag_Set(1, 3, 0);
    U16(G() + 0xB10) = 0; U8(G() + 0xB12) = 0;
    if (U8(G() + 0x108E)) {                       /* the extra game's timer */
        if (U16(G() + 0x138A) == 2) U16(G() + 0x138A) = 0;
        if (U16(G() + 0xC24) > 0x13 && U16(G() + 0x1090) != 1) U16(G() + 0x138A) = 1;
    }
    gptr name = Room_Name(U8(d + 0x2479), U8(d + 0x2478));
    U32(d + 0x2498) = name;
    U16(d + 0x2494) = (u16)Text_Left(name);
}

/* mode 1: the door's animation and the loading steps, the room name shown */
void DC2CALL Door_Run(gptr d)                                      /* 0x41E9C0 */
{
    gptr o = U32(G() + 0x940);
    if (!Flag_Check(1, 3)) U8(d + 0x247E) = (u8)Anim_Step(o);
    Orig_Call1(U32(A_DOOR_TYPES + 0xC + U8(d + 0x2480) * 0x20), d);
    Orig_Call1(U32(0x71FFD0 + U8(d + 0x2475) * 4), d);
    u8 t = U8(d + 0x2480);
    if (t == 0x58) Text_Draw(U32(d + 0x2498), U16(d + 0x2494), 0x68, 0x64, 0x808080, 5, 0);
    else if (t != 0x59) Text_Draw(U32(d + 0x2498), 0x10, 0x18, 0x64, 0x808080, 5, 0);
    if (U8(d + 0x2492)) Door_Score(d);
    else U8(d + 0x247D) |= 0x10;
    for (u32 i = 0; i < U32(0x87F940); i++, o += 0x260) {
        RotMatrix(o + 0x48, o + 0x20);
        Obj_UpdateMatrix(o);
    }
    Obj_PickLights(G() + 0x778);
}

/* step 0: switch area/room (the area's tables when it changes) */
void DC2CALL Door_AreaSwitch(gptr d)                               /* 0x41EAD0 */
{
    switch (U8(d + 0x2476)) {
    case 0:
        U8(d + 0x2476)++;
        if (U16(G() + 0x67C) != U8(d + 0x2479)) return;
        break;
    case 1:
        if (File_IsBusy()) return;
        U8(d + 0x2476)++;
        /* fall through */
    case 2:
        if (File_IsBusy()) return;
        break;
    default:
        return;
    }
    U8(G() + 0x10A9) = U8(G() + 0x1091);
    U8(G() + 0x1091) = U8(d + 0x2479);
    U8(G() + 0x10A8) = U8(G() + 0x1090);
    U8(G() + 0x1090) = U8(d + 0x2478);
    Orig_Call1(U32(A_AREAS + 4 + U8(G() + 0x1091) * 0x18), 0);
    U8(d + 0x2476) = 0;
    U8(d + 0x2475)++;
}

/* step 1: the room's file */
void DC2CALL Room_LoadTask(gptr d)                                 /* 0x41EBB0 */
{
    gptr rm = U32(A_AREAS + U8(G() + 0x1091) * 0x18) + U8(G() + 0x1090) * 0x18;
    switch (U8(d + 0x2476)) {
    case 0:
        if (File_IsBusy()) return;
        U8(d + 0x2476)++;
        File_RequestLoad(U16(rm + 2));
        /* fall through */
    case 1:
        if (File_IsBusy()) return;
        U16(G() + 0x67E) = U16(rm + 2);
        U8(d + 0x2476) = 0;
        U8(d + 0x2475)++;
        break;
    }
}

/* step 2: the room's music file unless already loaded; then done */
void DC2CALL Door_Music(gptr d)                                    /* 0x41EC50 */
{
    u32 v = Snd_RoomMusic();
    u16 f = 0;
    if (v != 0xFFFF) f = U16(U32(0x73842C + ((v >> 8) & 0xFF) * 4) + (v & 0xFF) * 2);
    switch (U8(d + 0x2476)) {
    case 0:
        if (f == U16(G() + 0x678) || v == 0xFFFF) { U8(d + 0x2476) = 2; return; }
        if (File_IsBusy()) return;
        File_RequestLoad(f);
        U8(d + 0x2476)++;
        break;
    case 1:
        if (File_IsBusy()) return;
        U16(G() + 0x678) = f;
        U8(d + 0x2476)++;
        break;
    case 2:
        U8(d + 0x247D) |= 1;
        break;
    }
}

/* mode 2: back to the game task, entering the room */
void DC2CALL Door_End(gptr d)                                      /* 0x41ED10 */
{
    (void)d;
    Flag_Set(1, 3, 0);
    U8(G() + 0xF) = 0;
    U16(G() + 0x664) = 2;
    U16(G() + 0x666) = 0; U16(G() + 0x668) = 0; U16(G() + 0x66A) = 0;
    Task_Chain(0x4510F0);
}

/* ---- the door types' per-frame handlers (0x71F1F0 +0xC). D+0x2477 is
 * their state; they fade out, play the scene, fade in, set flag 1/3 and
 * end the run mode once the music and the animation are done (0x11). The
 * stand-in is object 0, the door models objects 1 and 2. ---- */

#define DS(d) U8((d) + 0x2477)

/* the states every hinged door shares: fade out (0), fade in after the
 * animation (5), flag 1/3 once faded (6), the end (7) */
void DC2CALL Door_Common(gptr d)                                   /* 0x41CB70 */
{
    switch (DS(d)) {
    case 0:
        Fade_Set(0, 0x14, 1);
        DS(d)++;
        break;
    case 5:
        if (!U8(d + 0x247E)) break;
        Fade_Set(1, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 6:
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    case 7:
        if (U8(d + 0x247D) != 0x11) break;
        U8(d + 0x2474)++;
        Se_Play(0, 0x79);
        break;
    }
}

/* a hinged door: the stand-in pushes it (anim) and object 1 swings by step
 * from frame f1 until frame f2; character 2 (the dinosaur) skips it */
static void door_swing(gptr d, u32 anim, u32 f1, u32 f2, u32 step)
{
    gptr pl = U32(G() + 0x940);
    switch (DS(d)) {
    case 1:
        if (Flag_Check(2, 6)) return;
        if (U8(G() + 0x108F) == 2) { DS(d) = 3; U8(d + 0x247C) = 2; return; }
        DS(d)++;
        Anim_Start(pl, anim, 0, 0x18, 0xA);
        return;
    case 2:
        if (U16(pl + 0x92) < f1) return;
        DS(d)++;
        Se_Play(0, 0x78);
        /* fall through */
    case 3: {
        gptr o = U32(G() + 0x940) + 0x260;
        U16(o + 0x4A) += (u16)step;
        RotMatrix(o + 0x48, o + 0x20);
        if (U16(pl + 0x92) >= f2) DS(d) += 2;
        return;
    }
    }
}

void DC2CALL Door_SwingAMove(gptr d)                               /* 0x41CA70 */
{
    door_swing(d, U8(G() + 0x108F) ? 2 : 3, 0xB, 0x1D, 0x14);
}

void DC2CALL Door_SwingA(gptr d)                                   /* 0x41CA50 */
{
    Door_Common(d);
    Door_SwingAMove(d);
}

void DC2CALL Door_SwingBMove(gptr d)                               /* 0x41CC60 */
{
    door_swing(d, U8(G() + 0x108F) == 0, 0xA, 0x1C, 0x12);
}

void DC2CALL Door_SwingB(gptr d)                                   /* 0x41CC40 */
{
    Door_Common(d);
    Door_SwingBMove(d);
}

/* a shutter (object 1) rolled up by step, the stand-in walks under it */
void DC2CALL Door_Shutter(gptr d, s32 step)                        /* 0x41D090 */
{
    gptr pl = U32(G() + 0x940);
    switch (DS(d)) {
    case 0:
        Fade_Set(0, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 1:
        if (Flag_Check(2, 6)) break;
        goto sound;
    case 2:
        U16(pl + 0x2A2) -= (u16)step;
        if (S16(pl + 0x2A2) > -2000) break;
        DS(d)++;
        U32(pl + 0x88) = U32(d + 0x249C);
        Anim_Start(pl, 0x1A, 3, 0x19, 0xA);
        /* fall through */
    case 3:
        if (U16(pl + 0x92) < 6) break;
        DS(d)++;
        /* fall through */
    case 4:
        if (S16(pl + 0x44) <= 0) break;
    sound:
        DS(d)++;
        Se_Play(0, 0x78);
        break;
    case 5:
        U16(pl + 0x2A2) += (u16)step;
        if (S16(pl + 0x2A2) < -1000) break;
        Fade_Set(1, 0x14, 1);
        DS(d)++;
        break;
    case 6:
        U16(pl + 0x2A2) += (u16)step;
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    case 7:
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
    if (DS(d) > 3) U16(pl + 0x44) += 0x1B;
}

void DC2CALL Door_ShutterSlow(gptr d)                              /* 0x41D080 */
{
    Door_Shutter(d, 0x1E);
}

void DC2CALL Door_ShutterFast(gptr d)                              /* 0x41D210 */
{
    Door_Shutter(d, 0x50);
}

/* a sliding door: object 1 slides aside (dir: to -x), the stand-in walks
 * through, it slides back */
void DC2CALL Door_Slide(gptr d, u32 dir)                           /* 0x41D230 */
{
    gptr pl = U32(G() + 0x940);
    s16 v;
    u8 r = (u8)dir;
    switch (DS(d)) {
    case 0:
        Fade_Set(0, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 1:
        if (Flag_Check(2, 6)) break;
        DS(d)++;
        Se_Play(0, 0x78);
        break;
    case 2:
        U16(pl + 0x2A0) += r ? (u16)-0x32 : 0x32;
        v = S16(pl + 0x2A0);
        if (v > -1000 && v < 1000) break;
        DS(d)++;
        /* fall through */
    case 3:
        U32(pl + 0x88) = U32(d + 0x249C);
        Anim_Start(pl, 0x1A, 3, 0x19, 0xA);
        DS(d)++;
        /* fall through */
    case 4:
        if (U16(pl + 0x92) < 6) break;
        DS(d)++;
        /* fall through */
    case 5:
        if (S16(pl + 0x44) < 0x90) break;
        DS(d)++;
        Se_Play(0, 0x78);
        /* fall through */
    case 6: {
        gptr o = U32(G() + 0x940) + 0x260;
        U16(o + 0x40) += r ? 0x32 : (u16)-0x32;
        v = S16(o + 0x40);
        if (v > 0x258 || v < -0x258) break;
        DS(d)++;
        Fade_Set(1, 0xF, 1);
        break;
    }
    case 7:
        U16(pl + 0x2A0) += r ? 0x32 : (u16)-0x32;
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    case 8:
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
    if (DS(d) > 4) U16(pl + 0x44) += 0x1B;
}

void DC2CALL Door_SlideL(gptr d)                                   /* 0x41D220 */
{
    Door_Slide(d, 0);
}

void DC2CALL Door_SlideR(gptr d)                                   /* 0x41D420 */
{
    Door_Slide(d, 1);
}

/* the states the double doors share: fade out (0, 1), the end (8) */
void DC2CALL Door_Common2(gptr d)                                  /* 0x41D460 */
{
    gptr pl = U32(G() + 0x940);
    switch (DS(d)) {
    case 0:
        Fade_Set(0, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 1:
        if (Flag_Check(2, 6)) break;
        DS(d)++;
        Se_Play(0, 0x78);
        break;
    case 8:
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
    if (DS(d) > 4) U16(pl + 0x44) += 0x1B;
}

/* double sliding doors: objects 1 and 2 slide apart by step until lim,
 * the stand-in walks through, they close to lim2 */
void DC2CALL Door_Double(gptr d, s32 step, s32 lim, s32 lim2)      /* 0x41D4F0 */
{
    gptr pl = U32(G() + 0x940);
    s16 v;
    switch (DS(d)) {
    case 2:
        U16(pl + 0x2A0) -= (u16)step;
        U16(pl + 0x500) += (u16)step;
        v = S16(pl + 0x2A0);
        if (v > -(s32)(s16)lim && v < (s16)lim) break;
        DS(d)++;
        /* fall through */
    case 3:
        U32(pl + 0x88) = U32(d + 0x249C);
        Anim_Start(pl, 0x1A, 3, 0x19, 0xA);
        DS(d)++;
        /* fall through */
    case 4:
        if (U16(pl + 0x92) < 6) break;
        DS(d)++;
        /* fall through */
    case 5:
        if (S16(pl + 0x44) < 0x90) break;
        DS(d)++;
        Se_Play(0, 0x78);
        /* fall through */
    case 6: {
        gptr o = U32(G() + 0x940);
        U16(o + 0x2A0) += (u16)step;
        U16(o + 0x500) -= (u16)step;
        v = S16(o + 0x2A0);
        if (v > (s16)lim2 || v < -(s32)(s16)lim2) break;
        DS(d)++;
        Fade_Set(1, 0xF, 1);
        break;
    }
    case 7:
        U16(pl + 0x2A0) += (u16)step;
        U16(pl + 0x500) -= (u16)step;
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    }
}

void DC2CALL Door_DoubleA(gptr d)                                  /* 0x41D430 */
{
    Door_Common2(d);
    Door_Double(d, -0x32, 0x3E8, 0x320);
}

void DC2CALL Door_DoubleB(gptr d)                                  /* 0x41D670 */
{
    Door_Common2(d);
    Door_Double(d, 0x32, 0x3E8, 0x320);
}

void DC2CALL Door_DoubleC(gptr d)                                  /* 0x41D6A0 */
{
    Door_Common2(d);
    Door_Double(d, 0x1E, 0x258, 0x190);
}

/* double doors that stay open: they slide apart while the stand-in walks */
void DC2CALL Door_Double2(gptr d, s32 step)                        /* 0x41D6F0 */
{
    gptr pl = U32(G() + 0x940);
    s16 v;
    if (DS(d) > 1) {
        U16(pl + 0x2A0) -= (u16)step;
        U16(pl + 0x500) += (u16)step;
    }
    switch (DS(d)) {
    case 2:
        v = S16(pl + 0x2A0);
        if (v > -0x258 && v < 0x258) break;
        DS(d)++;
        /* fall through */
    case 3:
        U32(pl + 0x88) = U32(d + 0x249C);
        Anim_Start(pl, 0x1A, 3, 0x19, 0xA);
        DS(d)++;
        break;
    case 4:
        if (U16(pl + 0x92) < 6) break;
        DS(d)++;
        /* fall through */
    case 5:
        v = S16(pl + 0x2A0);
        if (v > -0x578 && v < 0x578) break;
        DS(d)++;
        /* fall through */
    case 6:
        DS(d)++;
        Fade_Set(1, 0xF, 1);
        break;
    case 7:
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    }
}

void DC2CALL Door_Double2A(gptr d)                                 /* 0x41D6D0 */
{
    Door_Common2(d);
    Door_Double2(d, -0x14);
}

void DC2CALL Door_Double2B(gptr d)                                 /* 0x41D820 */
{
    Door_Common2(d);
    Door_Double2(d, -0x19);
}

void DC2CALL Door_Double2C(gptr d)                                 /* 0x41D840 */
{
    Door_Common2(d);
    Door_Double2(d, 0x19);
}

/* the camera looking at (x, y, z) from its eye (G+0x778: +0x28 target,
 * +0x38 eye; as Door_SetCamera) */
void DC2CALL Door_CamTarget(u32 x, u32 y, u32 z)                   /* 0x41CEA0 */
{
    gptr c = G() + 0x778;
    U16(c + 0x28) = (u16)x; U16(c + 0x2A) = (u16)y; U16(c + 0x2C) = (u16)z;
    Camera_Aim(c + 0x28, c + 0x38, c + 0x20, c + 0x30);
    gptr v = Tmp_Push(8);
    U16(v) = (u16)-U16(c + 0x20); U16(v + 2) = (u16)-U16(c + 0x22); U16(v + 4) = (u16)-U16(c + 0x24);
    RotMatrix(v, c);
    Tmp_Pop(8);
}

/* the camera's eye moved to (x, y, z) */
void DC2CALL Door_CamEye(u32 x, u32 y, u32 z)                      /* 0x41D990 */
{
    gptr c = G() + 0x778;
    U16(c + 0x38) = (u16)x; U16(c + 0x3A) = (u16)y; U16(c + 0x3C) = (u16)z;
    Camera_Aim(c + 0x28, c + 0x38, c + 0x20, c + 0x30);
    gptr v = Tmp_Push(8);
    U16(v) = (u16)-U16(c + 0x20); U16(v + 2) = (u16)-U16(c + 0x22); U16(v + 4) = (u16)-U16(c + 0x24);
    RotMatrix(v, c);
    Tmp_Pop(8);
}

/* an elevator: the stand-in, the cage (object 1) and the light ride by dy
 * until the threshold, the camera following */
static void door_lift(gptr d, s32 dy)
{
    gptr pl = U32(G() + 0x940);
    U16(pl + 0x42) += (u16)dy;
    U16(pl + 0x2A2) += (u16)dy;
    U16(G() + 0x12) += (u16)dy;
    switch (DS(d)) {
    case 0:
        U8(d + 0x247C) = 0;
        Fade_Set(0, 0x14, 1);
        DS(d)++;
        Se_Play(0, 0x78);
        /* fall through */
    case 1:
        if (Flag_Check(2, 6)) break;
        DS(d)++;
        break;
    case 2:
        if (dy < 0 ? S16(pl + 0x42) > -8000 : S16(pl + 0x42) < 0x1F40) break;
        DS(d)++;
        /* fall through */
    case 3:
        Fade_Set(1, 0x14, 1);
        Se_Play(0, 0x79);
        DS(d)++;
        /* fall through */
    case 4:
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    case 5:
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
    Door_CamTarget(U16(pl + 0x40), (u16)(U16(pl + 0x42) - 0x400), U16(pl + 0x44));
}

void DC2CALL Door_LiftDown(gptr d)                                 /* 0x41CD60 */
{
    U16(G() + 0x7B0) += 0x18;
    door_lift(d, -0x30);
}

void DC2CALL Door_LiftUp(gptr d)                                   /* 0x41CF20 */
{
    gptr c = G() + 0x778;
    if (S16(U32(G() + 0x940) + 0x42) > 0xCE0) { U16(c + 0x3C) += 8; U16(c + 0x38) -= 0x10; }
    else U16(c + 0x3C) += 0x20;
    door_lift(d, 0x30);
}

/* the camera drops (eye y) and looks down */
void DC2CALL Door_Drop(gptr d)                                     /* 0x41D860 */
{
    gptr c = G() + 0x778;
    U16(c + 0x3A) -= 0x80;
    switch (DS(d)) {
    case 0:
        Fade_Set(0, 0x14, 1);
        Se_Play(0, 0x7A);
        DS(d)++;
        /* fall through */
    case 1:
        U8(d + 0x247C) = 0;
        if (Flag_Check(2, 6)) break;
        DS(d)++;
        break;
    case 2:
        if (S16(c + 0x3A) > -30000) break;
        DS(d)++;
        Se_StopGroup(0x7A);
        Se_Play(0, 0x79);
        /* fall through */
    case 3:
        Fade_Set(1, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 4:
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    case 5:
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
    Door_CamEye(U16(c + 0x38), U16(c + 0x3A), U16(c + 0x38));
}

/* no scene: a second's black (the objects hidden when hide) */
static void door_wait(gptr d, int hide)
{
    switch (DS(d)) {
    case 0:
        Fade_Set(0, 0x14, 1);
        DS(d)++;
        if (hide) U32(0x87F940) = 0;
        /* fall through */
    case 1:
        if (Flag_Check(2, 6)) break;
        U8(d + 0x247C) = 0;
        DS(d)++;
        break;
    case 2:
        if (++S8(d + 0x247C) < 0x3C) break;
        DS(d)++;
        /* fall through */
    case 3:
        Fade_Set(1, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 4:
        if (Flag_Check(2, 6)) break;
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
}

void DC2CALL Door_WaitHidden(gptr d)                               /* 0x41DA10 */
{
    door_wait(d, 1);
}

void DC2CALL Door_Wait(gptr d)                                     /* 0x41DB90 */
{
    door_wait(d, 0);
}

/* no scene: a message (to room 1 or elsewhere) until flag 2/0x1F clears */
void DC2CALL Door_Message(gptr d)                                  /* 0x41DAF0 */
{
    switch (DS(d)) {
    case 0:
        Msg_Reset();
        U32(d + 0x2498) = U16(d + 0x2478) == 1 ? 0x728A66u : 0x728ABCu;
        Message_Show(0, 0, U32(d + 0x2498), 2);
        DS(d)++;
        U32(0x87F940) = 0;
        /* fall through */
    case 1:
        Msg_Tick();
        if (Flag_Check(2, 0x1F)) break;
        DS(d)++;
        /* fall through */
    case 2:
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
}

/* the stand-in walks (footsteps from its animation's events) */
void DC2CALL Door_Walk(gptr d)                                     /* 0x41DC70 */
{
    gptr pl = U32(G() + 0x940);
    switch (DS(d)) {
    case 0:
        Fade_Set(0, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 1:
        if (Flag_Check(2, 6)) break;
        DS(d)++;
        break;
    case 2:
        U32(pl + 0x88) = U32(d + 0x249C);
        Anim_Start(pl, 0x1A, 3, 0x19, 0xA);
        DS(d)++;
        /* fall through */
    case 3:
        if (U16(pl + 0x92) < 6) break;
        DS(d)++;
        /* fall through */
    case 4:
        if (S16(pl + 0x44) < 0x300) break;
        Fade_Set(1, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 5:
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    case 6:
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
    if (DS(d) > 3) U16(pl + 0x44) += 0x1B;
    gptr e = U32(pl + 0x9C);
    if (!e || !U32(e)) return;
    gptr p = e + U32(e) * 4 + 4;
    for (u32 n = U32(e); n; n--) {
        p -= 4;
        if ((s32)U16(p) * 2 == S16(pl + 0xAA) + (s32)U16(pl + 0x92) * 2) Se_Play(0, 0x78);
    }
}

static void wait_busy(void)
{
    while (File_IsBusy()) Task_Sleep(1);
}

/* the stairs' first state: the character's stair animations loaded, the
 * stand-in re-made from the player's model, a random camera */
static void stairs_setup(gptr d, gptr pl)
{
    u16 f = U8(G() + 0x108F) ? 0x1F9 : 0x1E9;
    wait_busy();
    File_RequestLoad(f);
    wait_busy();
    U16(G() + 0x674) = f;
    DS(d)++;
    Obj_Free(pl);
    Model_Setup(pl, U32(pl + 0x60));
    U8(pl + 0xFE) = 3;
    Anim_Start(pl, U8(G() + 0x108F) == 0, 0, 1, 0);
    U16(pl + 0x102) &= ~0x10;
    U8(d + 0x247F) = Rand8() & 1;
}

/* stairs: the stand-in climbs (dy per frame, one more on the footsteps) */
void DC2CALL Door_StairsDown(gptr d)                               /* 0x41DDF0 */
{
    gptr pl = U32(G() + 0x940), c = G() + 0x778;
    switch (DS(d)) {
    case 0: {
        gptr cam = 0x71F020 + U8(A_DOOR_TYPES + U8(d + 0x2480) * 0x20 + 0x14) * 0x14;
        stairs_setup(d, pl);
        if (U8(d + 0x247F)) {
            U16(c + 0x38) = U16(cam); U16(c + 0x3A) = U16(cam + 2); U16(c + 0x3C) = U16(cam + 4);
        }
        if (!U8(G() + 0x108F)) { U16(pl + 0x44) = 0xFEB0; U16(pl + 0x42) = 0xFAFA; }
        Fade_Set(0, 0x14, 1);
    }
        /* fall through */
    case 1:
        if (Flag_Check(2, 6)) break;
        DS(d)++;
        break;
    case 2:
        if (S16(pl + 0x42) > -2750) break;
        DS(d)++;
        /* fall through */
    case 3:
        if (!U8(d + 0x247E)) break;
        Fade_Set(1, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 4:
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    case 5:
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
    U16(c + 0x38) += 4;
    u16 fr = U16(pl + 0x92);
    U16(pl + 0x42) -= 0x12;
    if (fr == 0 || fr == 0xC || fr == 0x18) U16(pl + 0x42) -= 1;
    if (U8(d + 0x247F)) U16(c + 0x3A) -= 0x10;
    Door_CamTarget(U16(pl + 0x40), (u16)(U16(pl + 0x42) - 0x100), U16(pl + 0x44));
}

void DC2CALL Door_StairsUp(gptr d)                                 /* 0x41E050 */
{
    gptr pl = U32(G() + 0x940), c = G() + 0x778;
    switch (DS(d)) {
    case 0:
        stairs_setup(d, pl);
        if (U8(d + 0x247F)) {
            U16(c + 0x38) = U16(0x71F034); U16(c + 0x3A) = U16(0x71F036); U16(c + 0x3C) = U16(0x71F038);
        } else U16(U32(G() + 0x940) + 0x2AA) += 0x800;
        if (!U8(G() + 0x108F)) { U16(pl + 0x42) -= 0x40; U16(pl + 0x44) += 0x50; }
        Fade_Set(0, 0x14, 1);
        /* fall through */
    case 1:
        if (Flag_Check(2, 6)) break;
        DS(d)++;
        break;
    case 2:
        if (S16(pl + 0x42) < -2000) break;
        DS(d)++;
        /* fall through */
    case 3:
        if (!U8(d + 0x247E)) break;
        Fade_Set(1, 0x14, 1);
        DS(d)++;
        /* fall through */
    case 4:
        if (Flag_Check(2, 6)) break;
        Flag_Set(1, 3, 1);
        DS(d)++;
        break;
    case 5:
        if (U8(d + 0x247D) == 0x11) U8(d + 0x2474)++;
        break;
    }
    U16(c + 0x38) += 4;
    u16 fr = U16(pl + 0x92);
    U16(pl + 0x42) += 0x12;
    if (fr == 0 || fr == 0xC || fr == 0x18) U16(pl + 0x42) += 1;
    if (U8(d + 0x247F)) {
        U16(c + 0x3A) += 8;
        Door_CamTarget(0x100, (u16)(U16(pl + 0x42) - 0x100), 0);
    } else Door_CamTarget(U16(pl + 0x40), (u16)(U16(pl + 0x42) - 0xC0), (u16)(U16(pl + 0x44) + 0x40));
}

/* the score panel's box */
void DC2CALL Door_ScoreBox(gptr d)                                 /* 0x41F040 */
{
    (void)d;
    Fn_41F080(0xCD, U16(G() + 0xC20) ? 0x96 : 0xAE, 0, 0xC0, 0, 0x808080);
}

/* the score panel after a fight (D+0x2491 counts down from 5): the combo
 * bonus (G+0xC20) graded by the kills (G+0xC22, 0x71FF68), the points
 * (G+0xC14) and the kills (G+0xC24) counted up, the bonus added */
void DC2CALL Door_Score(gptr d)                                    /* 0x41ED70 */
{
    gptr g = G();
    switch (S8(d + 0x2491)) {
    case 0:
        if (U16(d + 0x2488)++ > 0x28) U8(d + 0x247D) |= 0x10;
        break;
    case 1:
        U32(d + 0x248C) += U32(g + 0xC14) / 15 + 1;
        if (U32(d + 0x248C) < U32(g + 0xC14)) break;
        U32(d + 0x248C) = U32(g + 0xC14);
        U8(d + 0x2491)--;
        break;
    case 2: {
        u16 n = U16(g + 0xC24);
        if (n > 1) {
            U16(d + 0x248A) += (u16)(n / 15 + 1);
            if (U16(d + 0x248A) < n) break;
            U16(d + 0x248A) = n;
        }
        U8(d + 0x2491)--;
        break;
    }
    case 3: {
        u32 b = U16(g + 0xC20);
        if (!b) { U8(d + 0x2491)--; break; }
        U32(d + 0x2484) += b / 15 + 1;
        if (U32(d + 0x2484) < b) break;
        U32(d + 0x2484) = b;
        U32(g + 0x120C) += b;
        U32(g + 0x11F4) += U32(d + 0x2484);
        U32(g + 0xC14) += U32(d + 0x2484);
        if (S32(g + 0x11F4) > 9999999) U32(g + 0x11F4) = 9999999;
        U8(d + 0x2491)--;
        break;
    }
    case 4:
        if (U16(d + 0x2488)++ > 0xA) U8(d + 0x2491)--;
        break;
    case 5: {
        u16 t = U16(g + 0xC22), b = U16(g + 0xC20);
        gptr e = 0x71FF68 + t * 6;
        if (!b || t >= 0xF) U16(g + 0xC20) = 0;
        else if (b >= U8(e + 5)) U16(g + 0xC20) = U16(e + 2);
        else if (b >= U8(e + 4)) U16(g + 0xC20) = U16(e);
        else U16(g + 0xC20) = 0;
        U8(d + 0x2491)--;
        break;
    }
    }
    Door_ScoreBox(d);
    switch (S8(d + 0x2491)) {
    case 0: case 1:
        Fn_41F3E0(d);
        /* fall through */
    case 2:
        Fn_41F390(d);
        /* fall through */
    case 3:
        if (U16(g + 0xC20)) Fn_41F170(d);
        break;
    }
}

void DC2CALL Door_Task(void)                                       /* 0x41E2D0 */
{
    memset(MEM(DOOR), 0, 0x928 * 4);
    for (;;) {
        Orig_Call1(U32(0x71FFC4 + U8(DOOR + 0x2474) * 4), DOOR);
        Fn_4A7250();
        Task_Sleep(1);
    }
}
