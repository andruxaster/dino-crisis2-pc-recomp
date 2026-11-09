/* dc2 -- the game started the way WinMain 0x470190 does, without any code
 * of Dino2.exe: data sections of the exe as the memory image (temporary),
 * files from DATA_DIR, the root task on the task system, and the per-frame
 * game work of the main loop. Nothing is drawn or played yet; the run is
 * reported as a trace (root mode changes, files, tasks).
 * Buttons can be pressed from the command line (the pad is not read yet):
 * FRAME:MASK sets the "pressed this frame" word 0x87DC00 for that frame
 * (0x820 confirm, 0x40 back, 0x1000 up, 0x4000 down). In play, FROM-TO:MASK
 * holds game buttons (0x87DC08) over frames FROM..TO, pressed (0x87DC94) on
 * the first (0x1000 forward, 0x4000 back, 0x2000/0x8000 turn, 0x20 action).
 * DC2_TRACE=1 prints the player every 50 frames and the room's trigger areas.
 *   usage: dc2 Dino2.exe DATA_DIR [FRAMES] [FRAME:MASK | FROM-TO:MASK ...] */
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <setjmp.h>
#include <signal.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"

u8 *g_membase;

static int load_image(const char *exe)
{
    FILE *f = fopen(exe, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(n);
    if (fread(d, 1, n, f) != (size_t)n) { fclose(f); free(d); return 0; }
    fclose(f);
    u32 pe = *(u32 *)(d + 0x3C); u16 nsec = *(u16 *)(d + pe + 6), opt = *(u16 *)(d + pe + 20);
    g_membase = calloc(1, IMAGE_SIZE + TMP_SIZE + HEAP_SIZE);
    for (int i = 0; i < nsec; i++) {
        u8 *s = d + pe + 24 + opt + 40 * i;
        u32 va = *(u32 *)(s + 12), rs = *(u32 *)(s + 16), ro = *(u32 *)(s + 20), ch = *(u32 *)(s + 36);
        if (!(ch & 0x20000000)) memcpy(g_membase + va, d + ro, rs);   /* data sections only */
    }
    free(d);
    return 1;
}

/* game code reads data that code not rewritten yet would have set up (the
   player's model, ...): a fault ends the run with a report */
static sigjmp_buf g_fault;
static volatile int g_fault_sig;
static void on_fault(int sig) { g_fault_sig = sig; siglongjmp(g_fault, 1); }

/* the game part of one frame of WinMain's loop (graphics and timing left out) */
static void game_frame(void)
{
    gptr g = G();
    U32(g + 4)++;
    U32(0x87F94C) = 0x87DCC0 + U32(0x87DE0C) * 0x90;
    U32(0x87DC98) = U32(0x87DE0C) ? 0x500000 : 0x550000;
    Snd_Tick();
    if (!Flag_Check(2, 0x17)) File_Tick();
    if (!Flag_Check(2, 0x22)) {
        u32 d = U32(0x87DE00) ? U32(0x87DE00) : 1;
        U32(G() + 0x1098) += d;
        if (U32(G() + 0x1098) > 0x14996C4) U32(G() + 0x1098) = 0x14996C4;
    }
    if (!U8(0x74D74B)) Fn_478BF0(); else U8(0x74D74B)--;
    if (Flag_Check(2, 5)) { printf("reset requested (not handled yet)\n"); return; }
    if (!U8(0x74D749)) {
        Task_RunAll();
        Fn_450AF0();
        Fn_471AC0();
    }
    U32(0x87DE0C) ^= 1;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s Dino2.exe DATA_DIR [FRAMES]\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (!load_image(argv[1])) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    static char root[1024]; snprintf(root, sizeof root, "%s", argv[2]);
    g_pc_root = dirname(root);
    int frames = argc > 3 ? atoi(argv[3]) : 600;
    int npress = 0, press_at[32]; u32 press_mask[32];
    int nhold = 0, hold_from[32], hold_to[32]; u32 hold_mask[32];
    for (int i = 4; i < argc; i++) {
        if (nhold < 32 && sscanf(argv[i], "%d-%d:%x", &hold_from[nhold], &hold_to[nhold], &hold_mask[nhold]) == 3) nhold++;
        else if (npress < 32 && sscanf(argv[i], "%d:%x", &press_at[npress], &press_mask[npress]) == 2) npress++;
    }
    int trace = getenv("DC2_TRACE") != NULL;

    U32(A_G_PTR) = A_G_STATIC;
    Task_InitStackBase();
    Fade_Set(1, 1, 1);
    Snd_ReleaseAll();
    Task_Start(0, U8(0x87DE08) ? 0x471EC0 : 0x46D710);

    static u32 last_mode = 0xFFFFFFFF, last_file = 0xFFFFFFFF, last_tasks = 0xFFFFFFFF;
    static int f;
    if (sigsetjmp(g_fault, 1)) {
        printf("frame %4d: stopped by %s: data of code not rewritten yet (game mode %u, room %X%02X)\n", f,
               g_fault_sig == SIGFPE ? "SIGFPE" : "SIGSEGV", U16(G() + 0x664), U8(G() + 0x1091), U8(G() + 0x1090));
        return 2;
    }
    signal(SIGSEGV, on_fault); signal(SIGFPE, on_fault);
    for (f = 0; f < frames; f++) {
        if (U16(0x87FFEA) && U16(0x87FFE8) != last_file) {
            last_file = U16(0x87FFE8);
            gptr nm = U32(0x71B230 + last_file * 4);
            printf("frame %4d: load file %u %s\n", f, last_file, nm ? (const char *)MEM(nm) : "?");
        }
        U32(0x87DC00) = 0;
        for (int k = 0; k < npress; k++) if (press_at[k] == f) { U32(0x87DC00) = press_mask[k]; printf("frame %4d: press %X\n", f, press_mask[k]); }
        u32 held = 0, pressed = 0;
        for (int k = 0; k < nhold; k++) if (f >= hold_from[k] && f <= hold_to[k]) {
            held |= hold_mask[k];
            if (f == hold_from[k]) { pressed |= hold_mask[k]; printf("frame %4d: hold %X until %d\n", f, hold_mask[k], hold_to[k]); }
        }
        if (nhold) { U32(0x87DC08) = held; U32(0x87DC94) = pressed; }
        game_frame();
        u32 mode = U8(0x74D608) | U8(0x74D60A) << 8;
        if (mode != last_mode) { printf("frame %4d: root mode %u step %u\n", f, mode & 0xFF, mode >> 8); last_mode = mode; }
        static u32 last_title = 0xFFFFFFFF;
        u32 title = U16(G() + 0x76E) | U16(G() + 0x770) << 8 | U16(G() + 0x772) << 16;
        if (U16(G() + 0x76C) && title != last_title) {        /* once the title runs */
            printf("frame %4d: title screen %u step %u/%u cursor %d\n", f, title & 0xFF, (title >> 8) & 0xFF, title >> 16, S8(0x759EF2));
            last_title = title;
        }
        static u32 last_game = 0xFFFFFFFF;
        u32 gm = U16(G() + 0x664) | U8(G() + 0x1091) << 16 | U8(G() + 0x1090) << 8;
        if (gm != last_game && U16(G() + 0x664)) {
            printf("frame %4d: game mode %u, room ST%X%02X\n", f, gm & 0xFF, gm >> 16, (gm >> 8) & 0xFF);
            last_game = gm;
        }
        if (trace && f % 50 == 0 && U16(G() + 0x664) == 3 && U8(0x87F940)) {
            gptr pl = U32(G() + 0x940);
            printf("frame %4d: player %d,%d,%d yaw %03X state %X level %u\n", f, S16(pl + 0x40), S16(pl + 0x42), S16(pl + 0x44),
                   U16(pl + 0x4A), U32(pl + 0x54), U16(pl + 0x5A));
            for (int t = 0; t < 10; t++) {
                gptr th = U32(0x87FC20) + t * 0xA4;
                if (U8(th + 0x9B)) printf("  script thread %d state %X at %X: op %02X\n", t, U8(th + 0x9B), U32(th), U8(U32(th)));
            }
            printf("  flags 1: %08X 2: %08X\n", U32(U32(0x723C48)), U32(U32(0x723C50)));
            gptr set = U32(G() + 0xA68), st = G() + 0xA80;
            if (set) printf("  enemy set n %u type %u alive %u/%u group %u timer %u max %u found %u\n", U8(set), U8(set + 1), U8(st + 4), U8(set + 2),
                            U8(st + 9), U16(st), U8(st + 5), U8(st + 0xC));
            for (u32 k = 1; k < U32(0x87F940); k++) {
                gptr o = pl + k * 0x260;
                if (U8(o + 0x50) & 1)
                    printf("  object %u type %u state %X at %d,%d,%d yaw %03X health %d\n", k, U8(o + 0x58), U32(o + 0x54),
                           S16(o + 0x40), S16(o + 0x42), S16(o + 0x44), U16(o + 0x4A), S16(o + 0x108));
            }
        }
        static gptr last_aot;
        if (trace && U16(G() + 0x664) == 3 && U32(G() + 0xA5C) == 0x7593B8 && U8(U32(G() + 0xA5C)) && last_aot != U32(G() + 0xA5C) + U8(G() + 0x1090)) {
            last_aot = U32(G() + 0xA5C) + U8(G() + 0x1090);
            for (gptr e = U32(G() + 0xA5C); e < U32(G() + 0xA5C) + 0x500; e += 0x28) if (U8(e) & 1)
                printf("frame %4d: aot %2u flags %02X kind %u level %u quad %d,%d %d,%d %d,%d %d,%d\n", f,
                       (e - U32(G() + 0xA5C)) / 0x28, U8(e), U8(e + 1), U8(e + 2), S16(e + 4), S16(e + 6), S16(e + 8),
                       S16(e + 0xA), S16(e + 0xC), S16(e + 0xE), S16(e + 0x10), S16(e + 0x12));
        }
        u32 tasks = 0;
        for (int i = 0; i < 4; i++) tasks |= (u32)(U16(A_TASK_SLOTS + i * 0x20) != 0) << i;
        if (tasks != last_tasks) { printf("frame %4d: tasks running %X\n", f, tasks); last_tasks = tasks; }
        if (!tasks) { printf("frame %4d: no task left\n", f); break; }
    }
    return 0;
}
