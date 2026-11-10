/* Loading of models, stages and rooms, and the room set-up that follows:
 * section pointers of the room block, object arrays, script start, camera
 * zones. Loads wait for the file loader with Task_Sleep (the caller runs as
 * a cooperative task). */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/gte.h"
#include "dc2/task.h"

#define A_ENEMY_FILES  0x703520u   /* stride 8: u16 file per object type */
#define A_KOF_FILES    0x703598u   /* stride 8: playable dinosaurs (type - 0x1D) */
#define A_TEX_RAPTOR   0x703E48u   /* u32 file per variant (type 2), 10 */
#define A_TEX_OVIRAPT  0x703E70u   /* type 7, 9 */
#define A_TEX_GIGA     0x703E94u   /* type 6, 2 */

u32 DC2CALL File_IsBusy(void) { return U16(0x87FFEA); }            /* 0x403A20 */

static void load_wait(u32 id)
{
    while (File_IsBusy()) Task_Sleep(1);
    File_RequestLoad(id);
    while (File_IsBusy()) Task_Sleep(1);
}

/* model file of an object type and the texture file of a variant; G+0x680
 * type and G+0x682 variant loaded now */
void DC2CALL Enemy_LoadModel(u32 type, u32 variant)                 /* 0x482070 */
{
    gptr g = G();
    if (type <= 1 || type > 15) {
        if (type < 0x1D) return;
        type -= 0x1D;                             /* playable dinosaurs */
        load_wait(U16(A_KOF_FILES + type * 8));
        U16(G() + 0x680) = (u16)type;
        U16(G() + 0x682) = (u16)variant;
        return;
    }
    Flag_Set(2, 0x16, 1);
    u32 v = variant;
    g = G();
    if (v & 0x40000000) v = U16(g + 0x682);
    else {
        v &= 0xF;
        if (type == 2) v = S16(g + 0x138A) ? 9 : (u32)(S16(g + 0x1388) * 3) + v;
        else if (type == 7) v = (u32)(S16(g + 0x1388) * 3) + v;
        else if (type != 6) v = 0;
    }
    if (v == U16(g + 0x682) && type == U16(g + 0x680)) { Flag_Set(2, 0x16, 0); return; }
    if (type != U16(g + 0x680)) {
        U16(g + 0x682) = 0xFFFF;
        load_wait(U16(A_ENEMY_FILES + type * 8));
        g = G();
    }
    if (v != U16(g + 0x682)) {
        u32 tex = 0xFFFFFFFF;
        if (type == 2) { if (v > 9) v = 9; tex = U32(A_TEX_RAPTOR + v * 4); }
        else if (type == 7) { if (v > 8) v = 8; tex = U32(A_TEX_OVIRAPT + v * 4); }
        else if (type == 6) { if (v > 1) v = 1; tex = U32(A_TEX_GIGA + v * 4); }
        else goto done;
        if (tex != 0xFFFFFFFF) {
            U32(0x87DC04)++;
            load_wait(tex);
            g = G();
        }
    }
done:
    U16(g + 0x680) = (u16)type;
    U16(G() + 0x682) = (u16)v;
    Flag_Set(2, 0x16, 0);
}

/* stage files: CORE00+stage, the weapon set of the stage (weapon w) and the
 * sub-weapon s; then the player model is rebuilt */
void DC2CALL Stage_Load(u32 w, u32 s)                               /* 0x4822D0 */
{
    gptr pl = U32(G() + 0x940);
    Eff_KillKind(1); Eff_KillKind(2); Eff_KillKind(6);
    Eff_FreeAll();
    u8 st = U8(G() + 0x108F);
    Flag_Set(2, 0x20, st <= 2 ? 0 : st <= 4 ? 1 : st <= 6 ? 0 : 1);
    u32 core = 0x16 + U8(G() + 0x108F);
    if (U16(G() + 0x67A) != core) {
        load_wait(core);
        U16(G() + 0x67A) = (u16)core;
    }
    static const u16 wep[11] = {0x1FF, 0x205, 0x202, 0, 0, 0x200, 0x1FC, 0x200, 0x201, 0x1FA, 0x1FA};
    st = U8(G() + 0x108F);
    u32 base = (u32)(st - 2) <= 10 && wep[st - 2] ? wep[st - 2] : st ? 0x1F9 : 0x1E9;
    if (U16(G() + 0x674) != base + w) {
        load_wait(base + w);
        U16(G() + 0x674) = (u16)(base + w);
        U16(G() + 0x10B4) = (u16)((U16(G() + 0x10B4) & 0xFF00) + w);
        if (Flag_Check(9, 0x33) && U8(G() + 0x108F) < 2) {
            load_wait((U8(G() + 0x108F) ? 0x227 : 0x21D) + w);
        }
        st = U8(G() + 0x108F);
        if (st == 6) load_wait(0x231 + (w != 5));
        else if (st == 5) load_wait(0x233 + (w != 3));
    }
    if (U16(G() + 0x676) != s + 0x213) {
        load_wait(s + 0x213);
        U16(G() + 0x676) = (u16)(s + 0x213);
        U16(G() + 0x10B4) = (u16)(U8(G() + 0x10B4) + (s << 8));
    }
    if (U32(pl + 0x64)) Obj_Free(pl);
    Model_Setup(pl, 0x662500);
    Player_LoadWeapons(pl);
    Anim_Start(pl, 0x16, 0, 9, 0);
    U32(pl + 0x54) = 1;
    Obj_UpdateMatrix(pl);
    Obj_SavePos(pl);
    U8(G() + 0x75B) = 0;
    U8(G() + 0x75A) = 0;
}

/* ---- room set-up ------------------------------------------------------------------ */
void DC2CALL Room_Reset(void)                                       /* 0x481EC0 */
{
    U32(0x87F940) = 0; U32(0x87DC88) = 0; U32(0x87DE14) = 0; U32(0x87DE04) = 0; U32(0x87DC0C) = 0;
    for (gptr e = G() + 0x94C; e < G() + 0xA3C; e += 0x18) U8(e + 0xC) &= 0xFE;
}

/* object arrays from the counts in the script header, one block at hdr+0x2C */
void DC2CALL Room_AllocObjects(gptr hdr)                            /* 0x481F10 */
{
    gptr c = U32(hdr + 0x14), p = U32(hdr + 0x2C);
    u32 used, n;
    U32(0x87F940) = n = U8(c + 2) + 1u;
    U32(G() + 0x940) = p; used = n * 0x260; p += used;
    U32(G() + 0xA7C) = U8(c + 3);
    U32(0x87DC88) = n = U8(c + 4);
    U32(G() + 0x944) = p; p += n * 0xAC; used += n * 0xAC;
    U32(G() + 0xA70) = U8(c + 5);
    U32(0x87DE14) = n = U8(c + 6) + 1u;
    U32(G() + 0x948) = p; p += n * 0x34; used += n * 0x34;
    U32(G() + 0xA74) = U8(c + 7);
    U32(0x87DE04) = n = U8(c + 8) + 12u;
    U32(G() + 0x93C) = p; p += n * 0x70; used += n * 0x70;
    U32(G() + 0xA78) = U8(c + 9);
    n = U8(c + 0xB);
    U8(0x87FC48) = (u8)n;
    U32(A_SCR_THREADS) = p; p += n * 0xA4; used += n * 0xA4;
    U32(0x87DC0C) = U32(0x87F940) * 25;
    U32(G() + 0x938) = p;                         /* 0x13EC bytes per object follow */
    memset(MEM(U32(hdr + 0x2C)), 0, ((U32(0x87F940) * 0x13EC + used) >> 2) * 4);
}

void DC2CALL Room_SetPointers(gptr hdr)                             /* 0x481E10 */
{
    U32(G() + 0xA3C) = U32(hdr);           /* ambient */
    U32(G() + 0xA50) = U32(hdr + 4);       /* waypoints */
    U32(G() + 0xA4C) = U32(hdr + 8);       /* collision */
    U32(G() + 0xA44) = U32(hdr + 0xC);     /* camera zones */
    U32(G() + 0xA58) = U32(hdr + 0x10);
    U32(G() + 0xA54) = U32(hdr + 0x14);    /* script */
    U32(G() + 0xA64) = U32(hdr + 0x18);    /* footsteps */
    U32(G() + 0xA68) = 0;
    U32(G() + 0xA6C) = U32(hdr + 0x28);
    U32(G() + 0x7B8) = U32(hdr + 0x24);
    U32(G() + 0x7B8) += 0x10;              /* cameras */
    Room_AllocObjects(hdr);
}

void DC2CALL CamZones_Init(void)                                    /* 0x4034D0 */
{
    gptr g = G();
    U32(g + 0xA48) = U32(U32(g + 0xA44));
    U32(G() + 0xA44) = U32(g + 0xA44) + 4;
    U8(g + 0x7C4) = 0xFF;
    U8(g + 0x7C7) = 0;
    U8(G() + 0x84B) = 0xFF;
    U8(G() + 0x84A) = 0xFF;
}

void DC2CALL Aot_Reset(void)                                        /* 0x48AFD0 */
{
    U32(G() + 0xA5C) = 0x7593B8;
    for (gptr e = 0x7593B8; e < 0x7598B8; e += 0x28) U8(e) = 0;
}

/* ---- script modes: 0 run, 1 skip the cutscene, 2 start an event ------------------- */
void DC2CALL Script_Skip(void)                                      /* 0x48BCB0 */
{
    Cutscene_End();
    Shake_Set(0xFFFFFFFF, 0, 0);
    Flag_Set(2, 0xC, 0);
    for (u32 i = 0; i < 10; i++) Script_KillThread(i);
    Msg_Clear();
    Msg_ClearBox();
    Flag_Set(2, 0x1F, 0);
    for (u32 i = 0; i < U32(0x87DE14); i++) {
        gptr e = U32(G() + 0x948) + i * 0x34;
        if ((U8(e) & 1) && U8(e + 1) == 1) Eff_Off(e);
    }
    Lights_Unmark();
    Fade_Set(1, 5, 1);
    U8(0x87FC44)++;
}

void DC2CALL Script_Event(void)                                     /* 0x48BD60 */
{
    if (Flag_Check(2, 6)) return;
    U8(G() + 0xB12) = 2;
    U16(G() + 0xB10) = 0xFF;
    Script_StartThread(0, U8(0x87FC47));
    U8(0x87FC47) = 0xFF;
    U8(0x87FC44) = 0;
}

void DC2CALL Script_Tick(void)                                      /* 0x48BBF0 */
{
    if ((U32(0x87DC00) & 0x100) && U8(0x87FC47) != 0xFF && !Flag_Check(2, 6)) U8(0x87FC44) = 1;
    switch (U8(0x87FC44)) {
    case 0: Script_Run(); break;
    case 1: Script_Skip(); break;
    case 2: Script_Event(); break;
    default: Orig_Call1(U32(0x738DC0 + U8(0x87FC44) * 4), 0); break;
    }
    U16(0x87FC4A)++;
    U16(G() + 0xB24) = 0;
}

void DC2CALL Script_Init(void)                                      /* 0x48BB60 */
{
    U32(0x7598C0) = U32(G() + G_SCRIPT);
    U32(G() + G_SCRIPT) += 0x1C;
    memset(MEM(A_SCR_GLOBALS), 0, 0x8C);
    U8(0x87FC49) = 0;
    U8(0x87FC45) = 0xFF;
    U8(0x87FC47) = 0xFF;
    U8(G() + 0xB4A) = 0xFF;
    U32(A_SCR_GLOBALS) = U32(G() + G_SCRIPT) + U32(U32(0x7598C0) + 0xC);
    Aot_Reset();
    memset(MEM(U32(A_SCR_THREADS)), 0, 0x19A * 4);
    Script_StartThread(0, 0);
    Script_Tick();
}

/* ---- AOT (trigger areas) each frame ------------------------------------------------ */
#define A_AOT_KINDS 0x738B68u

/* the player against the active trigger areas of its level: kind 0 needs the
 * action button (flag 2/1), kind 8 the check button (2/0xD), kind 0xC the
 * camera's event (2/0xA, the event's type at 0x7593B0) and its own position
 * 0x7598B8; bit 2 = run its handler as a trigger, bit 1 = test the point
 * 0x190 in front of the player; bit 6 = also while climbing */
void DC2CALL AOT_Check(void)                                        /* 0x48B040 */
{
    gptr g = G(), pl = U32(g + 0x940), aot = U32(g + 0xA5C), end = aot + 0x500;
    u32 lv = U16(g + 0x10B4) >> 8;
    int climb = U8(pl + 0x55) == 0xA && (lv == 3 || lv == 6);
    for (gptr e = aot; e < end; e += 0x28) {
        u8 b = U8(e);
        if (!(b & 1)) continue;
        u16 level = (b & 0xC) == 0xC ? (u16)(-(s32)S16(0x7598BA) / 1000) : U16(pl + 0x5A);
        if (U8(e + 2) != level) continue;
        if (climb && !(b & 0x40)) continue;
        switch (b & 0xC) {
        case 0:
            if (Flag_Check(1, 0) || !Flag_Check(2, 1)) continue;
            break;
        case 8:
            if (Flag_Check(1, 0) || !Flag_Check(2, 0xD)) continue;
            break;
        case 0xC:
            if (!Flag_Check(2, 0xA)) continue;
            switch (U8(e) & 0xF0) {
            case 0x10: if (U16(0x7593B0) != 0x10) continue; break;
            case 0x20: if (U16(0x7593B0) != 0x20) continue; break;
            case 0x30: if (U8(0x7593B0) != 0x30) continue; break;
            }
            break;
        }
        gptr pos, v = 0;
        b = U8(e);
        if ((b & 0xC) == 0xC) pos = 0x7598B8;
        else if (b & 2) {
            v = Tmp_Push(8);
            U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x190;
            RotMatrix(pl + 0x48, pl + 0x20);
            ApplyMatrixSV(pl + 0x20, v, v);
            U16(v) += U16(pl + 0x40); U16(v + 2) += U16(pl + 0x42); U16(v + 4) += U16(pl + 0x44);
            pos = v;
        } else pos = pl + 0x40;
        u32 in = PointInQuad(pos, e + 4);
        if (v) Tmp_Pop(8);
        if (!in) continue;
        Orig_Call1(U32(A_AOT_KINDS + U8(e + 1) * 4), e);
        if (!(U8(e) & 4)) return;
    }
}

/* each frame: the pressed-area list cleared, the areas checked while the
 * player is in its normal state, the buttons' flags cleared */
void DC2CALL Aot_Tick(void)                                         /* 0x48AFF0 */
{
    gptr g = G();
    memset(MEM(g + 0xB28), 0, 0x20);
    if (U8(U32(g + 0x940) + 0x54) == 1) AOT_Check();
    Flag_Set(2, 1, 0);
    Flag_Set(2, 0xD, 0);
    Flag_Set(2, 0xA, 0);
}

/* AOT kind 0, a door: locked (+0x20 message, +0x21 key flag of group 8)
 * shows its message; else the destination (+0x14) is kept, the areas
 * cleared and the game task becomes the door task */
void DC2CALL AOT_Door(gptr e)                                       /* 0x48B390 */
{
    u8 key = U8(e + 0x21), msg = U8(e + 0x20), b = U8(e);
    if ((b & 0xC) != 0xC && (b & 0x80)) U8(G() + 0x926) = 1;
    if (msg && !Flag_Check(8, key)) {
        gptr w = Message_Show(0, 0, U32(0x738C28 + msg * 4), 3);
        if ((U8(e) & 0xC) == 8) U8(w) |= 2;
        return;
    }
    U32(0x7593B4) = e + 0x14;
    for (u32 k = 0; k < 0x500; k += 0x28) U8(U32(G() + 0xA5C) + k) = 0;
    Shake_Set(0xFFFFFFFF, 0, 0);
    Fn_4A7250();
    Task_Chain(0x41E2D0);
}

/* AOT kind 1, an event: starts script routine +0x18 in thread +0x14 */
void DC2CALL AOT_Event(gptr e)                                      /* 0x48B460 */
{
    Script_StartThread(U8(e + 0x14), U16(e + 0x18));
}

/* AOT kind 2, a message (+0x14 its offset in the room's script data) */
void DC2CALL AOT_Message(gptr e)                                    /* 0x48B480 */
{
    gptr w = Message_Show(0x1E, 0xBE, U32(G() + 0xA54) + U32(e + 0x14), 3);
    if ((U8(e) & 0xC) == 8) U8(w) |= 2;
}

/* AOT kind 3: sets flag +0x15 of group +0x14 to +0x16 */
void DC2CALL AOT_Flag(gptr e)                                       /* 0x48B4C0 */
{
    Flag_Set(U8(e + 0x14), U8(e + 0x15), U8(e + 0x16));
}

/* AOT kind 5, an item: the menu that picks it up (the entry's index and
   +0x1B in the last argument) */
void DC2CALL AOT_Item(gptr e)                                       /* 0x48B4E0 */
{
    u32 idx = (u32)((s32)(e - U32(G() + 0xA5C)) / 0x28) | (u32)U8(e + 0x1B) << 8;
    Menu_Open(1, (e & 0xFFFF0000u) | U16(e + 0x14), U8(e + 0x1A), U16(e + 0x18), idx);
}

/* AOT kind 6: object effect +0x14 starts (flag 1/0) */
void DC2CALL AOT_EffectStart(gptr e)                                /* 0x48B530 */
{
    Flag_Set(1, 0, 1);
    U8(U32(G() + 0x93C) + U32(e + 0x14) * 0x70 + 0x4E) = 2;
    U8(U32(G() + 0x93C) + U32(e + 0x14) * 0x70 + 0x46) = 1;
}

/* AOT kind 7: flag 1/3, and 3 into 0x87FC44 */
void DC2CALL AOT_Kind7(gptr e)                                      /* 0x48B590 */
{
    Flag_Set(1, 3, 1);
    U8(0x87FC44) = 3;
}

/* AOT kind 8: menu 2 */
void DC2CALL AOT_Menu(gptr e)                                       /* 0x48B5B0 */
{
    Menu_Open(2, 0, 0, 0, 0);
}

/* AOT kind 9: once its flag (+0x19 of group 0xA) is the one waited for
   (G+0xB4A), starts routine +0x14 in thread +0x18 */
void DC2CALL AOT_Wait(gptr e)                                       /* 0x48B5D0 */
{
    if (U8(G() + 0xB4A) == U8(e + 0x19)) {
        U8(G() + 0xB4A) = 0xFF;
        Script_StartThread(U8(e + 0x18), U16(e + 0x14));
    }
    Flag_Set(0xA, U8(e + 0x19), 1);
}

/* the last free object effect slot from the end, cleared */
gptr DC2CALL Eff_AllocBack(void)                                    /* 0x48D6C0 */
{
    u32 n = U32(0x87DE04) - U32(G() + 0xA78) - 0xC;
    gptr e = U32(G() + 0x93C) + U32(0x87DE04) * 0x70 - 0x70;
    for (; n; n--, e -= 0x70) {
        if (U8(e + 0x50) & 1) continue;
        memset(MEM(e), 0, 0x70);
        U32(e + 0x50) = 1;
        return e;
    }
    return 0;
}

/* AOT kind 10, by the character (G+0x10B5): Regina (1/7) with step 2 lets
   out eight object effects from her position (0x7598B8) in a ring
   (0x738C28 back +0x16 * 0x30), counting +0x16 down; Dylan (2/8) with step 1
   darkens effect slot +0x18; done: flag 8/+0x15 */
void DC2CALL AOT_Kind10(gptr e)                                     /* 0x48B620 */
{
    u16 ch = U8(G() + 0x10B5);
    if (ch == 1 || ch == 7) {
        if (U8(e + 0x14) != 2) return;
        Se_Play(U32(G() + 0x940), 0x26);
        gptr t = 0x738C28 - U8(e + 0x16) * 0x30;
        for (int k = 0; k < 8; k++, t += 6) {
            gptr f = Eff_AllocBack();
            if (!f) continue;
            U8(f + 0x4E) = 8;
            U32(f + 0x50) |= 7;
            U32(f + 0x68) = U32(e + 0x18);
            U16(f + 0x40) = U16(0x7598B8);
            U16(f + 0x42) = (u16)(U16(t) + U16(0x7598BA));
            U16(f + 0x44) = U16(0x7598BC);
            U16(f + 0x4C) = (U16(U32(G() + 0x940) + 0x4A) + U16(t + 2)) & 0xFFF;
            U8(f + 0x4F) = U8(t + 4);
        }
        u8 n = --U8(e + 0x16);
        if (n == 0) {
            Flag_Set(8, U8(e + 0x15), 1);
            U8(e) = 0;
            U8(U32(G() + 0x944) + U8(e + 0x17) * 0xAC + 0x54) = 3;
        } else if (n == 2) U8(U32(G() + 0x944) + U8(e + 0x17) * 0xAC + 0x54) = 2;
    } else if (ch == 2 || ch == 8) {
        if (U8(e + 0x14) != 1) return;
        gptr k2 = U32(G() + 0x944) + U32(e + 0x18) * 0xAC;
        U8(k2 + 0x64) = 0; U8(k2 + 0x65) = 0xFF; U8(k2 + 0x66) = 0;
        Se_Play(U32(G() + 0x940), 0x27);
        Flag_Set(8, U8(e + 0x15), 1);
        U8(e) = 0;
    }
}

/* AOT kind 11: one of two messages by flag 8/+0x14 */
void DC2CALL AOT_Kind11(gptr e)                                     /* 0x48B7D0 */
{
    Message_Show(0x1E, 0xBE, Flag_Check(8, U8(e + 0x14)) ? 0x72888C : 0x728692, 3);
}
