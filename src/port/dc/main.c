/* The Dreamcast build's entry (KallistiOS): the game started as dc2.c starts
 * it on the PC, its frames paced at the game's rate (0x87DE00 sixtieths a
 * frame), the pad read from the first controller, the picture drawn by the
 * PowerVR2 (render_pvr.c), the sound mixed into a KOS stream.
 *
 * Data: the game's Data folder under DC2_DC_ROOT (default /cd: the disc's
 * root holds DATA/; /pc/<dir> for dcload). Built with DC2_LOWMEM: the music
 * streams stay on the disc for now (silent), containers are read record by
 * record (src/game/file.c).
 *
 * Switches (Makefile.dreamcast): DC2_DC_ROOT, DC2_DC_AUDIO (0: no sound),
 * DC2_DC_MOVIES (0: the movies skipped), DC2_DC_VERBOSE (1, the default for
 * now: the memory in use every 300 frames on the serial port / dcload
 * console). */
#ifndef DC2_REV
#define DC2_REV "unknown revision"
#endif
#include <kos/dbgio.h>
#include <kos.h>
#include <dc/vblank.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <arch/irq.h>
#include <kos/thread.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/sound/stream.h>
#include "dc2/game.h"
#include "dc2/pcinput.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"
#include "dc2/render.h"
#include "dc2/data.h"
#include "../port.h"
#include "dc2/pcdraw.h"
#include "render_pvr.h"
#include "aica_sfx.h"
#include "ctx_sh4.h"
#include "vmu_save.h"
#include "unaligned.h"
#include "dc2/stall.h"
#include "prof.h"

unsigned g_stall_us[STALL_N];

#ifndef DC2_DC_ROOT
#define DC2_DC_ROOT "/cd"
#endif
#ifndef DC2_DC_AUDIO
#define DC2_DC_AUDIO 1
#endif
#ifndef DC2_DC_MOVIES
#define DC2_DC_MOVIES 0
#endif
#ifndef DC2_DC_VERBOSE
#define DC2_DC_VERBOSE 1
#endif

KOS_INIT_FLAGS(INIT_DEFAULT);

u8 *g_membase;

/* ---- the task system's stack switches (ctx_sh4.h) ------------------------------------- */
static void *g_thd_stack;
static size_t g_thd_size;
uint32_t dc2_ctx_enter(void *stack, uint32_t size)
{
    uint32_t irq = irq_disable();
    kthread_t *t = thd_get_current();
    if (!g_thd_stack) { g_thd_stack = t->stack; g_thd_size = t->stack_size; }
    if (stack) { t->stack = stack; t->stack_size = size; }
    else { t->stack = g_thd_stack; t->stack_size = g_thd_size; }
    return irq;
}
void dc2_ctx_leave_irq(uint32_t irq) { irq_restore(irq); }

/* ---- the pad ---------------------------------------------------------------------------
 * The game's pad bits (its default keys, 0x73BDB8, and joystick buttons,
 * 0x73BEB8): 0x1000 up, 0x4000 down, 0x8000 left, 0x2000 right; 0x20 action /
 * confirm, 0x40 cancel, 0x800 start (pause), 0x100 the status screen, 0x01 the
 * map, 0x10, 0x04, 0x08, 0x02 the PC's V, G, Shift, D. */
/* A GD-ROM (a .gdi in GDEMU or an emulator): KallistiOS's ISO 9660 driver
   reads the table of contents of the low-density area only, finds track 1
   (a small note at sector 0) and mounts that as /cd. On a GD-ROM the game
   is track 3 in the high-density area (from sector 45000), so the driver's
   call (linked through -Wl,--wrap=cdrom_read_toc) asks for that area's
   table instead. A CD-R (.cdi) is left as it is. */
int __real_cdrom_read_toc(cd_toc_t *toc, bool high_density);
int __wrap_cdrom_read_toc(cd_toc_t *toc, bool high_density)
{
    int status, type;
    if (!high_density && cdrom_get_status(&status, &type) == ERR_OK && type == CD_GDROM) {
        int r = __real_cdrom_read_toc(toc, true);
        if (r == ERR_OK) return r;
    }
    return __real_cdrom_read_toc(toc, high_density);
}

/* ---- RAM ----------------------------------------------------------------------------------
 * 16 MB in all, and the menus' surfaces come and go: the camera cache
 * (src/game/camera.c) gives way when the free RAM falls under 2 MB, and at
 * once when a surface does not fit. A failed allocation is reported (its
 * size and who asked; through -Wl,--wrap=malloc,...) the first few times. */
int CamCache_Trim(void);
int CamCache_Clear(void);
extern int (*g_low_memory)(void);
static int low_memory(void) { int had = CamCache_Trim(); return CamCache_Clear() | had; }

/* between frames: a large block (a 512x512 background surface, 512 KB) still
   to be had? if not, the camera cache emptied, then every cached texture
   dropped (remade from VRAM when next drawn, as at a room change): the map
   screen over a room with many textures ran the RAM out (a 512 KB surface
   not made). Not done inside an allocation (g_low_memory), where a texture
   being drawn could be released under its user. */
static void keep_headroom(u32 f)
{
    enum { WANT = 768 << 10 };
    extern void *__real_malloc(size_t n);          /* (a failed try not reported as a failure) */
    void *p = __real_malloc(WANT);
    if (p) { free(p); return; }
    CamCache_Clear();
    if ((p = __real_malloc(WANT))) { free(p); return; }
    PcDraw_DropTextures();
    p = __real_malloc(WANT);
    free(p);
    printf("low memory at frame %u: cached textures dropped%s\n", (unsigned)f, p ? "" : ", still no 768 KB block");
}

static volatile u32 g_alloc_fails;
static void alloc_failed(const char *what, size_t n, void *caller)
{
    static int busy;
    if (busy || g_alloc_fails++ >= 8) return;
    busy = 1;
    printf("%s(%u) failed, called from %08X\n", what, (unsigned)n, (unsigned)(uintptr_t)caller);
    busy = 0;
}
void *__real_malloc(size_t n);
void *__real_calloc(size_t a, size_t b);
void *__real_memalign(size_t al, size_t n);
void *__real_realloc(void *p, size_t n);
void *__wrap_malloc(size_t n) { void *p = __real_malloc(n); if (!p && n) alloc_failed("malloc", n, __builtin_return_address(0)); return p; }
void *__wrap_calloc(size_t a, size_t b) { void *p = __real_calloc(a, b); if (!p && a && b) alloc_failed("calloc", a * b, __builtin_return_address(0)); return p; }
void *__wrap_memalign(size_t al, size_t n) { void *p = __real_memalign(al, n); if (!p && n) alloc_failed("memalign", n, __builtin_return_address(0)); return p; }
void *__wrap_realloc(void *q, size_t n) { void *p = __real_realloc(q, n); if (!p && n) alloc_failed("realloc", n, __builtin_return_address(0)); return p; }

/* the frame counter and the game's state, for the watchdog (prof.c) */
static volatile unsigned g_frame_now;

#if DC2_DC_CRUMBS
/* the breadcrumbs (dc2/crumb.h): straight to the debug output, unbuffered,
   so the last ones are in the log however the machine stops. Each frame
   starts a line "#frame": g game, 0-3 a task run, R/r disc read begun/done,
   J a JPEG, M/m sound data into sound RAM, A/a a command to the AICA, d
   drawing, B/E the PVR scene begun/ended, U a texture upload, w the wait */
static void crumb(int c) { char s[2] = {(char)c, 0}; dbgio_write_str(s); }
#endif
static void wd_state(void)
{
    printf("  game heap peak %u KB; disc reads tried again %u; %u sounds on the AICA\n",
           (unsigned)(Pc_HeapPeak() >> 10), (unsigned)Host_ReadFailures(), (unsigned)Aica_Count());
    Port_DumpState();
}

static u16 read_pad(int *quit)
{
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    if (!dev) return 0;
    cont_state_t *st = (cont_state_t *)maple_dev_status(dev);
    if (!st) return 0;
    u32 b = st->buttons;
    u16 p = 0;
    if ((b & CONT_DPAD_UP) || st->joyy < -64) p |= 0x1000;
    if ((b & CONT_DPAD_DOWN) || st->joyy > 64) p |= 0x4000;
    if ((b & CONT_DPAD_LEFT) || st->joyx < -64) p |= 0x8000;
    if ((b & CONT_DPAD_RIGHT) || st->joyx > 64) p |= 0x2000;
    if (b & CONT_A) p |= 0x20;
    if (b & CONT_X) p |= 0x40;                    /* (X cancels, B is the PC's V: the user's choice) */
    if (b & CONT_B) p |= 0x10;
    if (b & CONT_Y) p |= 0x100;
    if (st->ltrig > 128) p |= 0x04;
    if (st->rtrig > 128) p |= 0x08;
    if (b & CONT_START) {
        if (st->ltrig > 128 && st->rtrig > 128) { p &= ~0x0Cu; p |= 0x01; }   /* L+R+Start: the map */
        else p |= 0x800;
    }
    if (b & CONT_C) p |= 0x02;
    if (b & CONT_Z) p |= 0x01;
    if ((b & CONT_RESET_BUTTONS) == CONT_RESET_BUTTONS) *quit = 1;      /* A+B+X+Y+Start: back to the loader */
    return p;
}

/* ---- the sound: the KOS stream pulls the mix as it plays ------------------------------------
 * The stream's callback mixes the frames it asks for right there, in the sound
 * thread (src/pc/audio.c holds its lock), so the sound keeps going at its own
 * pace whatever a game frame costs; the game's calls only change the voices. */
#if DC2_DC_AUDIO
#define SND_RATE 22050
static s16 g_out[4096 * 2] __attribute__((aligned(32)));
static snd_stream_hnd_t g_strm = SND_STREAM_INVALID;
static volatile u32 g_mix_us, g_mix_calls;         /* (the report: the mixer's time) */

static void *stream_cb(snd_stream_hnd_t h, int req, int *got)
{
    (void)h;
    u32 want = (u32)req / 4;                       /* bytes of stereo s16 */
    if (want > 4096) want = 4096;
    uint64_t t0 = timer_us_gettime64();
    PcAudio_Mix(g_out, want);
    g_mix_us += (u32)(timer_us_gettime64() - t0); g_mix_calls++;
    *got = (int)(want * 4);
    return g_out;
}

static void *audio_thread(void *arg)
{
    (void)arg;
    for (;;) { snd_stream_poll(g_strm); thd_sleep(5); }
    return NULL;
}

static void audio_open(void)
{
    if (snd_stream_init() < 0) return;
    Aica_Init();                                   /* (the banks' sounds on the AICA's channels) */
    g_strm = snd_stream_alloc(stream_cb, SND_STREAM_BUFFER_MAX / 4);
    if (g_strm == SND_STREAM_INVALID) return;
    PcAudio_Init(SND_RATE, NULL);                  /* (no sink: mixed when the stream asks) */
    snd_stream_start(g_strm, SND_RATE, 1);
    kthread_attr_t a = {0};
    a.prio = PRIO_DEFAULT - 1;
    a.label = "dc2 audio";
    thd_create_ex(&a, audio_thread, NULL);
}
#endif

/* the movies' clock (movie.c, DC2_MOVIE_AVI) */
unsigned long long Port_TimeUs(void) { return timer_us_gettime64(); }

/* ---- pacing: the game's frame lasts 0x87DE00 sixtieths of a second --------------------
 * Counted in vertical blanks, not by a timer: the PVR shows a new picture
 * only at a blank (pvr_wait_ready at the scene's start waits for it), so a
 * frame paced by the clock to 33.3 ms began out of step with the blanks and
 * waited for one more at its drawing: ~37 ms a frame (27 fps) with the work
 * well under 33. A frame now ends at its blank: the last one's + the
 * frame's length in blanks (2 at 30 fps; the display's rate is measured at
 * the start, so a 50 Hz one counts its own). A late frame does not wait; a
 * whole frame late starts the count again from now (no catching up). */
static volatile u32 g_vbl;
static u32 g_vbl_hz = 60, g_vbl_next, g_vbl_on;
static void on_vblank(uint32_t code, void *data) { (void)code; (void)data; g_vbl++; }

static void pace_init(void)
{
    vblank_handler_add(on_vblank, NULL);
    u32 v = g_vbl;
    uint64_t t = timer_us_gettime64();
    while (g_vbl == v && timer_us_gettime64() - t < 100000) thd_pass();     /* (one blank to start on) */
    v = g_vbl; t = timer_us_gettime64();
    while (g_vbl - v < 10 && timer_us_gettime64() - t < 500000) thd_pass();
    u32 n = g_vbl - v;
    uint64_t us = timer_us_gettime64() - t;
    if (n >= 10 && us) g_vbl_hz = (u32)((n * 1000000ull + us / 2) / us);
    if (g_vbl_hz < 45 || g_vbl_hz > 65) g_vbl_hz = 60;                    /* (no blanks counted: as NTSC) */
    printf("display: %u Hz, the frames paced by its blanks\n", (unsigned)g_vbl_hz);
}

static u32 pace(void)                              /* the time waited (us) */
{
    u32 n = U32(0x87DE00) ? U32(0x87DE00) : 1;
    u32 per = (n * g_vbl_hz + 30) / 60;
    if (!per) per = 1;
    uint64_t w0 = timer_us_gettime64();
    if (!g_vbl_on) { g_vbl_next = g_vbl; g_vbl_on = 1; }
    g_vbl_next += per;
    if ((s32)(g_vbl - g_vbl_next) >= 0) {
        if (g_vbl - g_vbl_next >= per) g_vbl_next = g_vbl;
        return 0;
    }
    while ((s32)(g_vbl - g_vbl_next) < 0) {
        if ((s32)(g_vbl_next - g_vbl) > 1) thd_sleep(4); else thd_pass();
    }
    return (u32)(timer_us_gettime64() - w0);
}

/* the frames' times, reported every 300 frames: real rate against the
   game's, then the average and the worst of the game's work, the drawing
   (the PVR included) and the wait */
static struct { u32 n, game, draw, wait, game_max, draw_max; uint64_t t0; } g_tm;
static void timing_report(void)
{
    uint64_t now = timer_us_gettime64();
    if (g_tm.n && g_tm.t0) {
        u32 span = (u32)(now - g_tm.t0), want = U32(0x87DE00) ? U32(0x87DE00) : 1;
        printf("speed: %u.%u game frames/s (the game wants %u); game %u.%u ms (max %u), drawing %u.%u ms (max %u), waiting %u.%u ms",
               (unsigned)(g_tm.n * 10000000ull / span / 10), (unsigned)(g_tm.n * 10000000ull / span % 10), (unsigned)(60 / want),
               (unsigned)(g_tm.game / g_tm.n / 1000), (unsigned)(g_tm.game / g_tm.n / 100 % 10), (unsigned)(g_tm.game_max / 1000),
               (unsigned)(g_tm.draw / g_tm.n / 1000), (unsigned)(g_tm.draw / g_tm.n / 100 % 10), (unsigned)(g_tm.draw_max / 1000),
               (unsigned)(g_tm.wait / g_tm.n / 1000), (unsigned)(g_tm.wait / g_tm.n / 100 % 10));
#if DC2_DC_AUDIO
        printf("; sound mixing %u%% of the CPU; %u sounds on the AICA, %u KB of its RAM free", (unsigned)(g_mix_us / (span / 100 ? span / 100 : 1)),
               (unsigned)Aica_Count(), (unsigned)(Aica_Free() >> 10));
        g_mix_us = 0; g_mix_calls = 0;
#endif
        printf("\n");
    }
    memset(&g_tm, 0, sizeof g_tm);
    g_tm.t0 = now;
}

/* the image without the original's code (0x400000..IMAGE_DATA, ~930 KB that
   standalone code never reads: checked on Linux with dc2's DC2_GUARD_TEXT):
   g_membase points that far before the buffer */
static int load_image(void)
{
    u32 skip = IMAGE_DATA - IMAGE_BASE, n = IMAGE_SIZE - skip + TMP_SIZE + HEAP_SIZE;
    u8 *buf = memalign(32, n);
    if (!buf) return 0;
    memset(buf, 0, n);
    g_membase = buf - skip;
    Data_Init();
    if (!U32(0x866144)) { PcIn_DefaultKeys(); PcIn_DefaultButtons(); }
    return 1;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Dino Crisis 2 (recompilation) for the Dreamcast, %s, data in %s\n", DC2_REV, DC2_DC_ROOT);
    DcUnaligned_Init();                            /* the x86 data's rare unaligned accesses */
    if (!DC2_DC_MOVIES) setenv("DC2_NOMOVIES", "1", 1);
    vid_set_mode(DM_640x480, PM_RGB565);
    const Renderer *rend = RenderPvr_Open();
    if (!rend) { printf("PVR set-up failed\n"); return 1; }
    if (!load_image()) { printf("out of memory for the game's image (%u KB)\n", (unsigned)((IMAGE_SIZE - (IMAGE_DATA - IMAGE_BASE) + TMP_SIZE + HEAP_SIZE) >> 10)); return 1; }
    g_pc_root = DC2_DC_ROOT;
    {
        char probe[256];
        Pc_HostPath("data\\CORE.DAT", probe, sizeof probe);
        FILE *t = fopen(probe, "rb");
        if (!t) { printf("the game's Data folder was not found (%s)\n", probe); return 1; }
        fclose(t);
    }
    DcSave_Import();                               /* saves: /ram, mirrored to the VMU */
#if DC2_DC_AUDIO
    audio_open();
#endif
    PcDraw_InitStandalone(rend, 640, 480);

    U32(A_G_PTR) = A_G_STATIC;
    Port_GameReset();
    {   /* the options' display page: the television's one mode */
        static const u16 sizes[1][2] = {{640, 480}};
        Port_DisplayInit("Dreamcast", 0, sizes, 1, 0, 1, NULL);
    }
    Task_InitStackBase();
    Fade_Set(1, 1, 1);
    Snd_ReleaseAll();
    Task_Start(0, U8(0x87DE08) ? 0x471EC0 : 0x46D710);

    g_low_memory = low_memory;
    g_gog_fixes = 1;                                /* the GOG release's fixes to the game (docs/gog_patches.md) */
    pace_init();
#if DC2_DC_VERBOSE
    Prof_Init();
    Prof_Watchdog(&g_frame_now, wd_state);
#endif
    int quit = 0;
    for (u32 f = 0; !quit; f++, g_frame_now = f) {
#if DC2_DC_CRUMBS
        if (f == 0) g_crumb = crumb;
        fflush(stdout);
        { char s[16]; snprintf(s, sizeof s, "\n#%u ", (unsigned)f); dbgio_write_str(s); }
#endif
        g_pad_raw[0] = read_pad(&quit);
        CamCache_Trim();                           /* (the cache keeps 2 MB free for the rest) */
        if (f % 30 == 0) keep_headroom(f);
        memset(g_stall_us, 0, sizeof g_stall_us);
        uint64_t t0 = timer_us_gettime64();
        CRUMB('g');
        Port_GameFrame();
#if DC2_DC_VERBOSE
        Port_StuckCheck(f, g_pad_raw[0]);
        /* L+R+Y held for 2 s: the state into the log (to describe what is on screen) */
        static u32 lry;
        if ((g_pad_raw[0] & 0x10C) == 0x10C) { if (++lry == 60) { printf("state at frame %u (L+R+Y):\n", (unsigned)f); Port_DumpState(); } }
        else lry = 0;
#endif
        uint64_t t1 = timer_us_gettime64();
        CRUMB('d');
        Port_DrawFrame();
        CRUMB('w');
        uint64_t t2 = timer_us_gettime64();
        u32 w = pace();
        u32 tg = (u32)(t1 - t0), td = (u32)(t2 - t1);
        g_tm.n++; g_tm.game += tg; g_tm.draw += td; g_tm.wait += w;
        if (tg > g_tm.game_max) g_tm.game_max = tg;
        if (td > g_tm.draw_max) g_tm.draw_max = td;
#if DC2_DC_VERBOSE
        /* a frame over 100 ms (a stall: loading, uploads, the disc), a few a report */
        static u32 slow_n;
        Prof_FrameEnd(tg > 100000 || td > 100000);
        if (f % 300 == 0) slow_n = 0;
        if ((tg > 100000 || td > 100000) && slow_n++ < 4)
            printf("slow frame %u: game %u ms, drawing %u ms (disc %u, JPEG %u, textures %u ms; game mode %u room %X%02X)\n", (unsigned)f,
                   (unsigned)(tg / 1000), (unsigned)(td / 1000), g_stall_us[STALL_DISC] / 1000, g_stall_us[STALL_JPEG] / 1000, g_stall_us[STALL_TEX] / 1000,
                   (unsigned)U16(G() + 0x664), (unsigned)U8(G() + 0x1091), (unsigned)U8(G() + 0x1090));
#endif
#if DC2_DC_VERBOSE
        if (f % 300 == 0)
        {
            struct mallinfo mi = mallinfo();
            printf("frame %u: game mode %u room %X%02X; game heap peak %u KB of %u; surfaces peak %u KB; malloc in use %u KB; pvr free %u KB, %u textures dropped (%u for fragments); disc reads tried again %u\n",
                   (unsigned)f, (unsigned)U16(G() + 0x664), (unsigned)U8(G() + 0x1091), (unsigned)U8(G() + 0x1090),
                   (unsigned)(Pc_HeapPeak() >> 10), (unsigned)(HEAP_SIZE >> 10), (unsigned)(PcSoft_SurfacePeak() >> 10),
                   (unsigned)(mi.uordblks >> 10), (unsigned)(pvr_mem_available() >> 10), (unsigned)RenderPvr_Evicted(), (unsigned)RenderPvr_Fragmented(), (unsigned)Host_ReadFailures());
            DcUnaligned_Report();
            timing_report();
            if (f % 1800 == 0 && f) Prof_Report();
        }
#endif
        u32 tasks = 0;
        for (int i = 0; i < 4; i++) tasks |= (u32)(U16(A_TASK_SLOTS + i * 0x20) != 0) << i;
        if (!tasks) { printf("frame %u: no task left\n", (unsigned)f); break; }
    }
    return 0;
}
