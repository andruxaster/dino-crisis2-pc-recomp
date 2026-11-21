/* The root task and its boot sequence (0x46D710..), the save-slot scan
 * that leads to the title (0x403920), and the per-frame game work of the
 * main loop (inline in WinMain 0x470190). */
#include <stdio.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"

#define A_ROOT        0x74D608u   /* {u8 mode, u8 ?, u8 step, u8 skip, s16 timer} */
#define A_ROOT_MODES  0x723AC8u
#define A_SCREEN_FILES 0x703FD8u  /* stride 4: u16 file of a full-screen image */
#define SCREEN_BUF    0x6C1500u

static void load_wait(u32 id)
{
    while (File_IsBusy()) Task_Sleep(1);
    File_RequestLoad(id);
    while (File_IsBusy()) Task_Sleep(1);
}

/* full-screen picture n (logos, warnings) to 0x6C1500 */
void DC2CALL Screen_LoadImage(u32 n)                                /* 0x4828D0 */
{
    while (File_IsBusy()) Task_Sleep(1);
    File_RequestRaw(U16(A_SCREEN_FILES + n * 4), SCREEN_BUF);
    while (File_IsBusy()) Task_Sleep(1);
    Flag_Set(3, 0, 1);
    U32(G() + 8) = SCREEN_BUF;
}

void DC2CALL Root_Boot(gptr r)                                      /* 0x46D740  mode 0 */
{
    U8(r)++;
    U8(0x87DE08) = 1;
    U32(0x87DE00) = 0;
    Flag_Set(3, 0, 0);
    U32(G() + 8) = SCREEN_BUF;
    load_wait(0x15);                            /* CORE.DAT */
}

/* a picture shown for 180 frames between fades; mode 2 can be skipped */
static void show_picture(gptr r, u32 n, int skippable)
{
    switch (U8(r + 2)) {
    case 0:
        U8(r + 2)++;
        Screen_LoadImage(n);
        U16(r + 4) = 0xB4;
        Flag_Set(3, 0, 1);
        Fade_Set(0, 0x14, 1);
        break;
    case 1:
        if (!Flag_Check(2, 6)) U8(r + 2)++;     /* fade in done */
        break;
    case 2:
        U16(r + 4)--;
        if (skippable && (UU32(0x87DC12) & 0x820)) { U16(r + 4) = 0; U8(r + 3) = 1; }
        if (!U16(r + 4)) { U8(r + 2)++; Fade_Set(1, 0x14, 1); }
        break;
    case 3:
        if (!Flag_Check(2, 6)) { Flag_Set(3, 0, 0); U8(r + 2) = 0; U8(r)++; }
        break;
    default: break;
    }
}

void DC2CALL Root_Logo(gptr r) { show_picture(r, 9, 0); }          /* 0x46D7C0  mode 1 */

void DC2CALL Root_Warning(gptr r)                                   /* 0x46D880  mode 2 */
{
    if (U8(r + 3)) { U8(r)++; return; }
    show_picture(r, 7, 1);
}

void DC2CALL Root_ToTitle(gptr r)                                   /* 0x46D970  mode 3 */
{
    (void)r;
    U32(0x87DE00) = 2;
    Task_Chain(0x403920);
}

void DC2CALL RootTask(void)                                         /* 0x46D710 */
{
    U32(A_ROOT) = 0;
    for (;;) {
        u8 m = U8(A_ROOT);
        switch (m) {
        case 0: Root_Boot(A_ROOT); break;
        case 1: Root_Logo(A_ROOT); break;
        case 2: Root_Warning(A_ROOT); break;
        case 3: Root_ToTitle(A_ROOT); break;
        default: Orig_Call1(U32(A_ROOT_MODES + m * 4), A_ROOT); break;
        }
        Task_Sleep(1);
    }
}

/* which of the 100 save slots exist: SAVExx.DAT of 0x5F4 bytes, header
   (16 bytes at 0x184) to G+0x20+16*i; then the title */
void DC2CALL Save_ScanTask(void)                                    /* 0x403920 */
{
    U8(0x87DC90) = 0;
    gptr flags = 0x87F341, info = 0x20;
    for (u32 i = 0; flags < 0x87F4D1; i++, flags += 4, info += 0x10) {
        gptr path = Tmp_Push(0x14);
        char name[16];
        snprintf(name, sizeof name, "SAVE%02u.DAT", (unsigned)i);
        memcpy(MEM(path), name, strlen(name) + 1);
        U8(flags - 1) = 0; U8(flags) = 0; U8(flags + 1) = 0;
        s32 fd = Pc_Open(path, 0, 0);
        Tmp_Pop(0x14);
        if (fd != -1) {
            if (Pc_Size(fd) == 0x5F4) {
                Pc_SeekSet(fd, 0x184);
                Pc_Read(fd, G() + info, 0x10);
                U8(flags - 1) = 1; U8(flags) = 1;
                U8(0x87DC90) = 1;
            }
            Pc_Close(fd);
        } else {
            gptr g = G();
            U32(g + info) = 0; U32(g + info + 4) = 0; U32(g + info + 8) = 0; U32(g + info + 0xC) = 0;
        }
    }
    U32(G() + 0x664) = 0;
    U16(0x87DC08) = 0; U16(0x87DC94) = 0; U16(0x87DDFC) = 0; U16(0x87DC00) = 0;
    Task_Chain(0x4A5730);                       /* title */
}
