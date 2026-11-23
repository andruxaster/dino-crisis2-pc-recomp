/* The main loop's game work shared by the standalone builds (dc2.c on
 * Linux / Windows, dc/main.c on the Dreamcast): WinMain's frame without its
 * window, the run's start, and the graphics part of the loop. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dc2/game.h"
#include "dc2/pcinput.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"
#include "dc2/gpu.h"
#include "dc2/gte.h"
#include "dc2/pcdraw.h"
#include "port.h"

/* the game part of one frame of WinMain's loop (graphics and timing left out) */
void Port_GameFrame(void)
{
    gptr g = G();
    U32(g + 4)++;
    U32(0x87F94C) = 0x87DCC0 + U32(0x87DE0C) * 0x90;
    U32(0x87DC98) = U32(0x87DE0C) ? 0x500000 : 0x550000;
    PcGpu_ClearOTagR(U32(0x87F94C) + 0x70, 8);
    PcGpu_ClearOTagR(0x87BC00 + (U32(0x87DE0C) << 12), 0x400);
    PcGpu_ClearOTagR(0x87DDE0 + U32(0x87DE0C) * 12, 3);
    PcGpu_ClearOTagR(0x87F960 + (U32(0x87DE0C) << 4), 4);
    Snd_Tick();
    if (!Flag_Check(2, 0x17)) File_Tick();
    if (!Flag_Check(2, 0x22)) {
        u32 d = U32(0x87DE00) ? U32(0x87DE00) : 1;
        U32(G() + 0x1098) += d;
        if (U32(G() + 0x1098) > 0x14996C4) U32(G() + 0x1098) = 0x14996C4;
    }
    if (!U8(0x74D74B)) Input_Frame(); else U8(0x74D74B)--;
    if (Flag_Check(2, 5)) { printf("reset requested (not handled yet)\n"); return; }
    if (!U8(0x74D749)) {
        Task_RunAll();
        Fade_Tick();
        Msg_Flush();
    }
}

/* the start of a run (0x470980, also after a reset): the geometry screen,
   the game state cleared, the two display/drawing buffers (0,256 and 0,0) */
void Port_GameReset(void)
{
    SetGeomOffset(160, 120);
    SetGeomScreen(320);
    memset(MEM(G()), 0, 0x540 * 4);
    memcpy(MEM(G() + 0x14EC), MEM(0x723CCC), 16);   /* the options' defaults: stereo, music and sound volume 15 */
    PcGpu_SetDefDrawEnv(0x87DCD4, 0, 0, 320, 240);
    PcGpu_SetDefDispEnv(0x87DCC0, 0, 256, 320, 240);
    PcGpu_SetDefDrawEnv(0x87DD64, 0, 256, 320, 240);
    PcGpu_SetDefDispEnv(0x87DD50, 0, 0, 320, 240);
    U32(0x87DE00) = 2;
    U8(0x87DD7C) = 1; U8(0x87DCEC) = 1;
    Light_SetFar(0);
    PcDraw_SetFogEnable(0);
    PcIn_FixedKeys(0);
}

/* the graphics part of the main loop (0x47053E): the last frame shown, this
   one's background and order table drawn, the buffers swapped */
/* Rooms' textures: the small-memory builds (the Dreamcast) keep only the
   current room's, as the PS1 did: at a room change the PC layer's texture
   cache is dropped (its pages are made again from VRAM as they are drawn).
   Elsewhere only with DC2_DROPTEX set (a test). */
static void room_textures(void)
{
    static int on = -1;
    static u32 room = 0xFFFFFFFF;
#ifdef DC2_LOWMEM
    on = 1;
#else
    if (on < 0) on = getenv("DC2_DROPTEX") != 0;
#endif
    if (!on) return;
    u32 r = U16(G() + 0x1090);
    if (r != room) {
        if (room != 0xFFFFFFFF) PcDraw_DropTextures();
        room = r;
    }
}

void Port_DrawFrame(void)
{
    room_textures();
    PcDraw_PutDispEnv(U32(0x87F94C));
    PcDraw_PutDrawEnv(U32(0x87F94C) + 0x14);
    if (Flag_Check(3, 0) && !U8(0x74D749)) {
        gptr g = G();
        PcDraw_Background(U32(g + 8), U8(g + 0xC26));
        if (U8(g + 0xB73)) { U8(g + 0xB73) = 0; U8(g + 0xC26) = 0; }
    }
    PcDraw_DrawOTag(U32(0x87F94C) + 0x8C);
    U32(0x87DE0C) ^= 1;
}


/* ---- the state reports ------------------------------------------------------------------
 * The fields are the ones DC2_TRACE prints (dc2.c): the player object (0x260
 * bytes each from G+0x940), the script threads (0x87FC20, 0xA4 each), the
 * flag words, the enemy set, the task slots with their entries (original
 * addresses, names in tools/dc2_labels.csv) and where they last slept (host
 * code: the build's symbols name it). */
extern gptr g_task_entry_now[4];
extern void *g_task_sleep_at[4];

void Port_DumpState(void)
{
    gptr g = G();
    printf("  root mode %u step %u; game mode %u room %X%02X; fade %d level %u; frame flags %08X %08X %08X\n",
           (unsigned)(U8(0x74D608)), (unsigned)(U8(0x74D60A)), (unsigned)(U16(g + 0x664)), (unsigned)(U8(g + 0x1091)), (unsigned)(U8(g + 0x1090)), (s8)U8(g + 0x814), (unsigned)(U32(g + 0x810) >> 16),
           (unsigned)(U32(U32(0x723C40))), (unsigned)(U32(U32(0x723C48))), (unsigned)(U32(U32(0x723C50))));
    for (int i = 0; i < 4; i++) {
        gptr s = A_TASK_SLOTS + i * 0x20;
        if (U16(s)) printf("  task %d state %u wait %u entry %06X slept at %p\n", (int)i, (unsigned)(U16(s)), (unsigned)(U16(s + 2)),
                           (unsigned)g_task_entry_now[i], g_task_sleep_at[i]);
    }
    if (U16(g + 0x664) == 3 && U8(0x87F940)) {
        gptr pl = U32(g + 0x940);
        printf("  player %d,%d,%d yaw %03X state %X level %u health %d\n", (int)S16(pl + 0x40), (int)S16(pl + 0x42), (int)S16(pl + 0x44),
               (unsigned)(U16(pl + 0x4A)), (unsigned)(U32(pl + 0x54)), (unsigned)(U16(pl + 0x5A)), (int)S16(pl + 0x108));
        for (int t = 0; t < 10; t++) {
            gptr th = U32(0x87FC20) + t * 0xA4;
            if (U8(th + 0x9B)) printf("  script thread %d state %X at %X: op %02X\n", (int)t, (unsigned)(U8(th + 0x9B)), (unsigned)(U32(th)), (unsigned)(U8(U32(th))));
        }
        gptr set = U32(g + 0xA68), st = g + 0xA80;
        if (set) printf("  enemy set n %u type %u alive %u/%u group %u timer %u max %u found %u\n", (unsigned)(U8(set)), (unsigned)(U8(set + 1)), (unsigned)(U8(st + 4)),
                        (unsigned)(U8(set + 2)), (unsigned)(U8(st + 9)), (unsigned)(U16(st)), (unsigned)(U8(st + 5)), (unsigned)(U8(st + 0xC)));
        for (u32 k = 1; k < U32(0x87F940) && k < 64; k++) {
            gptr o = pl + k * 0x260;
            if (U8(o + 0x50) & 1)
                printf("  object %u type %u state %X at %d,%d,%d yaw %03X health %d\n", (unsigned)(k), (unsigned)(U8(o + 0x58)), (unsigned)(U32(o + 0x54)),
                       (int)S16(o + 0x40), (int)S16(o + 0x42), (int)S16(o + 0x44), (unsigned)(U16(o + 0x4A)), (int)S16(o + 0x108));
        }
    }
}

/* a checksum of what moves while the game plays: the player, the script
   threads, the fade, the tasks' states (not their timers) */
static u32 game_signature(void)
{
    gptr g = G();
    u32 h = U16(g + 0x664) * 31u + U8(g + 0x1090) + (U8(g + 0x1091) << 8) + U8(0x74D608) * 7u + U8(0x74D60A) * 13u;
    h = h * 33u + U32(g + 0x810) + U8(g + 0x814);
    for (int i = 0; i < 4; i++) h = h * 33u + U16(A_TASK_SLOTS + i * 0x20) + (u32)g_task_entry_now[i];
    if (U16(g + 0x664) == 3 && U8(0x87F940)) {
        gptr pl = U32(g + 0x940);
        for (int k = 0x40; k < 0x60; k += 4) h = h * 33u + U32(pl + k);
        for (int t = 0; t < 10; t++) { gptr th = U32(0x87FC20) + t * 0xA4; h = h * 33u + U32(th) + U8(th + 0x9B); }
    }
    return h;
}

void Port_StuckCheck(u32 frame, u16 pad)
{
    static u32 last, same, pressed, told;
    u32 h = game_signature();
    if (h != last || U16(G() + 0x664) != 3) { last = h; same = pressed = 0; told = 0; return; }
    same++;
    if (pad) pressed++;
    if (same >= STUCK_FRAMES && pressed >= 30 && !told) {
        told = 1;
        printf("STUCK? frame %u: the game's state has not changed for %u frames, the pad pressed in %u of them\n", (unsigned)(frame), (unsigned)(same), (unsigned)(pressed));
        Port_DumpState();
    }
}

void Port_DisplayInit(const char *name, int can_window, const u16 (*sizes)[2], int n, int cur, int fullscreen, void (*apply)(void))
{
    extern void (*g_port_display_apply)(void);
    gptr d = 0x75DEFC;                                     /* device 0: +0 can run in a window, +0x14 its name */
    memset(MEM(d), 0, 0x164);
    U32(d) = can_window != 0;
    snprintf((char *)MEM(d + 0x14), 0x40, "%s", name);
    if (n > 0x40) n = 0x40;                                /* (0x204 bytes a list: a count, 8 bytes a mode) */
    for (u32 win = 0; win < 2; win++) {
        gptr l = 0x75BE70 + win * 4 * 0x204;
        memset(MEM(l), 0, 0x204);
        if (!win && !can_window) continue;
        S32(l) = n;
        for (int i = 0; i < n; i++) { U16(l + 4 + 8 * i) = sizes[i][0]; U16(l + 6 + 8 * i) = sizes[i][1]; U16(l + 8 + 8 * i) = 32; }
    }
    if (cur < 0 || cur >= n) cur = 0;
    U32(0x74D738) = 1;                                     /* (one device: the page does not cycle them) */
    U32(0x723CA0) = 0; U32(0x723CA4) = fullscreen || !can_window;
    U32(0x723CB0) = sizes[cur][0]; U32(0x723CB4) = sizes[cur][1]; U32(0x723CB8) = 32;
    U32(0x87FF9C) = U32(d); U32(0x87FFA0) = d + 0x14;
    g_port_display_apply = apply;
}
