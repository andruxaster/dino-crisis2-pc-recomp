/* smoke -- the reimplemented game code running natively (no Dino2.exe code).
 * Builds the game's memory image from the tables of src/data, loads a room with the game's
 * file loader (File_Process: room block LZSS to 0x5E0000, tables, sound,
 * textures to the PC layer, which is not rewritten yet), starts the room script like
 * Script_Init and runs Script_Run frame by frame until an opcode that is
 * not reimplemented yet stops the threads.
 * Functions that are not rewritten yet are reported once and skipped.
 *   usage: smoke DATA_DIR [ROOM|all] [FRAMES]
 * Each run prints the files the room's scripts load (cutscenes SCxx.DAT come through script opcode 4D,
 * Door_Load); a fault prints the faulting game address and every live
 * script thread (pc, opcode). Builds on Linux and with mingw-w64. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <strings.h>
#include <libgen.h>
#include <setjmp.h>
#include <signal.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/data.h"
#include "dc2/addr.h"

#ifdef _WIN32
/* mingw: no sigsetjmp/sigaction; the handler is re-armed before each room */
#define SMOKE_JMP          jmp_buf
#define SMOKE_SETJMP(b)    setjmp(b)
#define SMOKE_LONGJMP(b)   longjmp(b, 1)
#else
#define SMOKE_JMP          sigjmp_buf
#define SMOKE_SETJMP(b)    sigsetjmp(b, 1)
#define SMOKE_LONGJMP(b)   siglongjmp(b, 1)
#endif

#define A_FILE_ID     0x87FFE8u   /* u16 last file requested (src/game/file.c) */
#define A_FILE_COUNT  0x87DC9Cu   /* u32 requests so far */
#define A_FILE_NAMES  0x71B230u   /* char *[676] */

u8 *g_membase;

static int load_image(void)
{
    g_membase = calloc(1, IMAGE_SIZE + TMP_SIZE + HEAP_SIZE);
    if (!g_membase) return 0;
    Data_Init();
    return 1;
}

/* Game code reads data that loaders not rewritten yet would have put in
   memory (models, animations of cutscene files, ...). A fault there ends
   the room's run with a report instead of ending the program. */
static SMOKE_JMP g_fault_jmp;
static volatile int g_fault_sig;
static void *volatile g_fault_host;           /* host address of the fault (POSIX only) */
#ifdef _WIN32
static void on_fault(int sig) { g_fault_sig = sig; g_fault_host = NULL; SMOKE_LONGJMP(g_fault_jmp); }
static void arm_faults(void) { signal(SIGSEGV, on_fault); signal(SIGFPE, on_fault); }
#else
static void on_fault_info(int sig, siginfo_t *si, void *uc)
{
    (void)uc;
    g_fault_sig = sig;
    g_fault_host = si ? si->si_addr : NULL;
    SMOKE_LONGJMP(g_fault_jmp);
}
static void arm_faults(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fault_info;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}
#endif
static void disarm_faults(void)
{
    signal(SIGSEGV, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
#ifndef _WIN32
    signal(SIGBUS, SIG_DFL);
#endif
}

static const char *file_name(u32 id)
{
    if (id >= 676) return "?";
    gptr nm = U32(A_FILE_NAMES + 4 * id);
    return nm ? (const char *)MEM(nm) : "?";
}

/* the files requested since the room file: polled after each frame (loads
   inside a frame finish there, Task_Sleep runs the loader outside a task) */
static u32 g_files_seen;
static char g_files_log[1024];
static void note_files(int frame)
{
    u32 n = U32(A_FILE_COUNT);
    if (n == g_files_seen) return;
    size_t l = strlen(g_files_log);
    if (l < sizeof g_files_log - 48)
        snprintf(g_files_log + l, sizeof g_files_log - l, "%s%s@%d%s",
                 l ? " " : "", file_name(U16(A_FILE_ID)), frame,
                 n - g_files_seen > 1 ? "(+more)" : "");
    g_files_seen = n;
}

static void dump_threads(void)
{
    gptr th = U32(A_SCR_THREADS);
    for (int t = 0; t < SCR_NTHREADS; t++) {
        gptr p = th + t * SCR_THREAD_SIZE;
        u8 st = U8(p + T_STATE);
        if (!st) continue;
        gptr pc = U32(p + T_PC);
        int ok = pc >= IMAGE_BASE && pc < IMAGE_BASE + IMAGE_SIZE;
        printf("    thread %d: state %02X pc %08X op %s", t, st, pc, ok ? "" : "?");
        if (ok) printf("%02X", U8(pc));
        printf(" sel %02X/%02X\n", U8(p + T_SEL0), U8(p + T_SEL1));
    }
}

/* stand-in for the object type handlers (rewritten, but run by the frame loop): they build the
   object's model parts and set its animation table; give each object a
   chain of 32 empty parts and a one-frame animation so that the code which
   walks them has something to work on */
#define TYPE_TABLE 0x6B8000u
#define PARTS      0x6C0000u      /* 32 objects x 32 parts x 0x80 */
#define ANIM_TAB   0x6B8800u      /* 256 x -> ANIM_STUB */
#define ANIM_STUB  0x6B9000u      /* one-frame animation, all values 1 */
static void type_handler(gptr fn, u32 obj)
{
    (void)fn;
    gptr objs = U32(G() + 0x940);
    if (obj < objs || obj >= objs + 32 * 0x260 || (obj - objs) % 0x260) return;
    if (!U32(obj + 0x88)) U32(obj + 0x88) = ANIM_TAB;
    if (U32(obj + 0x64)) return;
    gptr p = PARTS + (obj - objs) / 0x260 * 0x1000, parent = obj;
    memset(MEM(p), 0, 0x1000);
    U32(obj + 0x64) = p;
    for (int i = 0; i < 32; i++, p += 0x80) {
        U32(p + 0x58) = parent;
        if (i < 31) U32(p + 0x54) = p + 0x80;
        parent = p;
    }
}

static int run_room(const char *data, const char *room, int frames)
{
    free(g_membase);
    if (!load_image()) { fprintf(stderr, "out of memory\n"); return 1; }
    U32(A_G_PTR) = A_G_STATIC;

    /* the room file through the game's own loader (File_Process) */
    char want[16]; snprintf(want, sizeof want, "%s.DAT", room);
    int id = -1;
    for (int i = 0; i < 676 && id < 0; i++) {
        gptr nm = U32(0x71B230 + 4 * i);
        if (nm && !strcasecmp((const char *)MEM(nm), want)) id = i;
    }
    if (id < 0) { fprintf(stderr, "%s: not in the file table\n", room); return 1; }
    U16(0x87FFEA) = 0;
    File_RequestLoad((u32)id);
    File_Process();
    if (U16(0x87FFEA)) { fprintf(stderr, "%s: cannot read %s\n", room, want); return 1; }

    static int f, maxalive, alive;
    f = maxalive = alive = 0;
    g_files_seen = U32(A_FILE_COUNT);
    g_files_log[0] = 0;
    if (SMOKE_SETJMP(g_fault_jmp)) {
        disarm_faults();
        const char *sig = g_fault_sig == SIGFPE ? "SIGFPE" : g_fault_sig == SIGSEGV ? "SIGSEGV" : "SIGBUS";
        printf("%s: stopped at frame %d by %s", room, f, sig);
        if (g_fault_host) {
            uintptr_t h = (uintptr_t)g_fault_host, b = (uintptr_t)g_membase;
            if (h >= b && h < b + IMAGE_SIZE + TMP_SIZE + HEAP_SIZE)
                printf(" at game address %08X", (u32)(h - b) + IMAGE_BASE);
            else if (h < 0x10000)
                printf(" at %p (null pointer)", g_fault_host);
            else
                printf(" at host %p (outside the image)", g_fault_host);
        }
        printf("\n    files: %s\n", g_files_log[0] ? g_files_log : "none");
        dump_threads();
        return 2;
    }
    arm_faults();
    /* room set-up as the game does it after a room load */
    Room_SetPointers(0x5E0000);                /* sections, object arrays after the block */
    CamZones_Init();
    /* stand-ins for what the object type handlers would build: the C handlers
       (Hook_Find) expect the parts and animations that the game's frame loop
       (Game_Frame, run by dc2) sets up, which smoke does not run */
    for (int i = 0; i < 256; i++) U32(ANIM_TAB + 4 * i) = ANIM_STUB;
    U32(ANIM_STUB) = 1;
    for (u32 a = ANIM_STUB + 0x14; a < ANIM_STUB + 0x1000; a += 2) U16(a) = 1;
    for (u32 i = 0; i < U32(0x87F940) && i < 32; i++) type_handler(0, U32(G() + 0x940) + i * 0x260);
    U32(0x87DCA0) = TYPE_TABLE;               /* object type handlers: stand-in */
    g_orig_call1 = type_handler;
    Script_Init();                             /* thread 0 on routine 0, first frame */
    note_files(0);

    for (f = 0; f < frames; f++) {
        Script_Run();
        note_files(f + 1);
        alive = 0;
        for (int t = 0; t < SCR_NTHREADS; t++)
            if (U8(U32(A_SCR_THREADS) + t * SCR_THREAD_SIZE + T_STATE)) alive++;
        if (alive > maxalive) maxalive = alive;
        if (!alive) break;
    }
    disarm_faults();
    printf("%s: %d frames, up to %d threads, %d running at the end\n", room, f, maxalive, alive);
    if (g_files_log[0]) printf("    files: %s\n", g_files_log);
    return 0;
}

static int cmp(const void *a, const void *b) { return strcmp(*(char **)a, *(char **)b); }

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s DATA_DIR [ROOM|all] [FRAMES]\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char root[1024]; snprintf(root, sizeof root, "%s", argv[1]);
    g_pc_root = dirname(root);                 /* "data\NAME" is looked up from here */
    const char *room = argc > 2 ? argv[2] : "ST100";
    int frames = argc > 3 ? atoi(argv[3]) : 120;
    if (strcmp(room, "all")) return run_room(argv[1], room, frames);
    DIR *dir = opendir(argv[1]);
    if (!dir) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    char *names[256]; int n = 0; struct dirent *e;
    while ((e = readdir(dir)) && n < 256) {
        size_t l = strlen(e->d_name);
        if (l == 9 && !strncmp(e->d_name, "ST", 2) && !strcmp(e->d_name + 5, ".DAT")) {
            char *s = malloc(6);                /* (no strndup in the Windows CRT) */
            if (!s) break;
            memcpy(s, e->d_name, 5); s[5] = 0;
            names[n++] = s;
        }
    }
    closedir(dir);
    qsort(names, n, sizeof *names, cmp);
    int bad = 0, stopped = 0;
    for (int i = 0; i < n; i++) {
        int r = run_room(argv[1], names[i], frames);
        if (r == 2) stopped++; else bad |= r;
    }
    printf("%d rooms, %d stopped by a fault\n", n, stopped);
    return bad;
}
