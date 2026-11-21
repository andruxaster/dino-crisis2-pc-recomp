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

