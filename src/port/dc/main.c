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
#include <kos.h>
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
#include "render_pvr.h"
#include "ctx_sh4.h"
#include "vmu_save.h"
#include "unaligned.h"

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
    if (b & CONT_B) p |= 0x40;
    if (b & CONT_X) p |= 0x10;
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

/* ---- the sound: the mixer's frames into a ring, the KOS stream takes them ---------------- */
#if DC2_DC_AUDIO
#define SND_RATE 22050
#define RING (SND_RATE / 2)                       /* half a second of stereo frames */
static s16 g_ring[RING * 2] __attribute__((aligned(32)));
static volatile u32 g_rd, g_wr;                    /* in frames, wrapping at RING */
static s16 g_out[4096 * 2] __attribute__((aligned(32)));
static snd_stream_hnd_t g_strm = SND_STREAM_INVALID;

static void audio_sink(const s16 *lr, u32 frames)
{
    for (u32 i = 0; i < frames; i++) {
        u32 w = g_wr, nx = (w + 1) % RING;
        if (nx == g_rd) break;                     /* full: the rest of the frame is dropped */
        g_ring[w * 2] = lr[i * 2]; g_ring[w * 2 + 1] = lr[i * 2 + 1];
        g_wr = nx;
    }
}

static void *stream_cb(snd_stream_hnd_t h, int req, int *got)
{
    (void)h;
    u32 want = (u32)req / 4;                       /* bytes of stereo s16 */
    if (want > 4096) want = 4096;
    for (u32 i = 0; i < want; i++) {
        if (g_rd == g_wr) { g_out[i * 2] = g_out[i * 2 + 1] = 0; continue; }
        g_out[i * 2] = g_ring[g_rd * 2]; g_out[i * 2 + 1] = g_ring[g_rd * 2 + 1];
        g_rd = (g_rd + 1) % RING;
    }
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
    g_strm = snd_stream_alloc(stream_cb, SND_STREAM_BUFFER_MAX / 4);
    if (g_strm == SND_STREAM_INVALID) return;
    PcAudio_Init(SND_RATE, audio_sink);
    snd_stream_start(g_strm, SND_RATE, 1);
    kthread_attr_t a = {0};
    a.prio = PRIO_DEFAULT - 1;
    a.label = "dc2 audio";
    thd_create_ex(&a, audio_thread, NULL);
}
#endif

/* ---- pacing: the game's frame lasts 0x87DE00 sixtieths of a second -------------------- */
static uint64_t g_next_us;
static void pace(void)
{
    u32 n = U32(0x87DE00) ? U32(0x87DE00) : 1;
    uint64_t now = timer_us_gettime64();
    if (!g_next_us || now > g_next_us + 8000) g_next_us = now;   /* (late: start again from now) */
    g_next_us += (uint64_t)n * 16667;
    while ((now = timer_us_gettime64()) < g_next_us) {
        if (g_next_us - now > 4000) thd_sleep(2); else thd_pass();
    }
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
    printf("Dino Crisis 2 (recompilation) for the Dreamcast, data in %s\n", DC2_DC_ROOT);
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
    Task_InitStackBase();
    Fade_Set(1, 1, 1);
    Snd_ReleaseAll();
    Task_Start(0, U8(0x87DE08) ? 0x471EC0 : 0x46D710);

    int quit = 0;
    for (u32 f = 0; !quit; f++) {
        g_pad_raw[0] = read_pad(&quit);
        Port_GameFrame();
        PcAudio_Frame(U32(0x87DE00));
        Port_DrawFrame();
        pace();
#if DC2_DC_VERBOSE
        if (f % 300 == 0)
        {
            struct mallinfo mi = mallinfo();
            printf("frame %u: game mode %u room %X%02X; game heap peak %u KB of %u; surfaces peak %u KB; malloc in use %u KB; pvr free %u KB\n",
                   (unsigned)f, (unsigned)U16(G() + 0x664), (unsigned)U8(G() + 0x1091), (unsigned)U8(G() + 0x1090),
                   (unsigned)(Pc_HeapPeak() >> 10), (unsigned)(HEAP_SIZE >> 10), (unsigned)(PcSoft_SurfacePeak() >> 10),
                   (unsigned)(mi.uordblks >> 10), (unsigned)(pvr_mem_available() >> 10));
            DcUnaligned_Report();
        }
#endif
        u32 tasks = 0;
        for (int i = 0; i < 4; i++) tasks |= (u32)(U16(A_TASK_SLOTS + i * 0x20) != 0) << i;
        if (!tasks) { printf("frame %u: no task left\n", (unsigned)f); break; }
    }
    return 0;
}
