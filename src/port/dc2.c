/* dc2 -- the game started the way WinMain 0x470190 does, without any code
 * of Dino2.exe: the memory image from the tables of src/data, files from
 * DATA_DIR, the root task on the task system, and the per-frame
 * game work of the main loop. Nothing is drawn or played yet; the run is
 * reported as a trace (root mode changes, files, tasks).
 * Buttons can be pressed from the command line: the pad (0x87DC42, read by
 * Input_Frame 0x478BF0) holds MASK for one frame with FRAME:MASK (0x820
 * confirm, 0x40 back, 0x1000 up, 0x4000 down) or over frames FROM..TO with
 * FROM-TO:MASK (0x1000 forward, 0x4000 back, 0x2000/0x8000 turn, 0x20 action).
 * DC2_TRACE=1 prints the player every 50 frames, the room's trigger areas, the
 * screen's fades and the background masks.
 *   usage: dc2 DATA_DIR [FRAMES] [FRAME:MASK | FROM-TO:MASK ...] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <setjmp.h>
#include <signal.h>
#include "dc2/game.h"
#include "dc2/pcinput.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"
#include "dc2/gpu.h"
#include "dc2/gte.h"
#include "dc2/pcdraw.h"
#include "dc2/render.h"
#include "dc2/data.h"
#ifdef DC2_GL
#include <SDL2/SDL.h>
#include "render_gl.h"
#endif

u8 *g_membase;

/* the memory image: zero, the tables of src/data at their addresses */
static int load_image(void)
{
    g_membase = calloc(1, IMAGE_SIZE + TMP_SIZE + HEAP_SIZE);
    if (!g_membase) return 0;
    Data_Init();
    return 1;
}

/* game code reads data that code not rewritten yet would have set up (the
   player's model, ...): a fault ends the run with a report */
static sigjmp_buf g_fault;
static volatile int g_fault_sig;
static void on_fault(int sig) { g_fault_sig = sig; siglongjmp(g_fault, 1); }

#ifdef DC2_GL
/* DC2_STATS: per frame, the draws and what the frame looks like in numbers
   (mean colour, share of non-black pixels) */
static const Renderer *g_rend;
static void gl_stats(RenderGlStats *s)
{
    static u16 px[640 * 480];
    g_rend->read_back(px, 640, 480, 1280);
    unsigned long long sr = 0, sg = 0, sb = 0; u32 lit = 0;
    for (u32 i = 0; i < 640 * 480; i++) {
        u16 c = px[i]; u32 r5 = c >> 10 & 31, g5 = c >> 5 & 31, b5 = c & 31;
        sr += r5; sg += g5; sb += b5; if (r5 | g5 | b5) lit++;
    }
    printf("gl frame %u: %u draws, %u vertices, %u uploads; mean rgb %u,%u,%u (of 31), %u%% lit\n", s->frames, s->draws, s->verts,
           s->uploads, (u32)(sr / (640 * 480)), (u32)(sg / (640 * 480)), (u32)(sb / (640 * 480)), lit * 100 / (640 * 480));
}
#endif

#ifdef DC2_GL
/* the sound: each frame's samples queued (not past a fifth of a second
   ahead, so that running fast does not pile them up) */
static SDL_AudioDeviceID g_audio;
static void audio_sink(const s16 *lr, u32 frames)
{
    if (!g_audio || SDL_GetQueuedAudioSize(g_audio) > 44100 * 4 / 5) return;
    SDL_QueueAudio(g_audio, lr, frames * 4);
}
static void audio_open(void)
{
    if (getenv("DC2_MUTE") || SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    SDL_AudioSpec want = {0}, got;
    want.freq = 44100; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
    g_audio = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (!g_audio) { fprintf(stderr, "SDL audio: %s\n", SDL_GetError()); return; }
    PcAudio_Init(44100, audio_sink);
    SDL_PauseAudioDevice(g_audio, 0);
}
#endif

/* DC2_WAV=file: the sound written to a wave file (also headless) */
static FILE *g_wav;
static u32 g_wav_frames;
static void wav_header(void)
{
    u32 bytes = g_wav_frames * 4, h[11] = {0x46464952, 36 + bytes, 0x45564157, 0x20746D66, 16, 0x00020001, 44100, 44100 * 4, 0x00100004, 0x61746164, bytes};
    fseek(g_wav, 0, SEEK_SET); fwrite(h, 4, 11, g_wav); fseek(g_wav, 0, SEEK_END);
}
static void wav_sink(const s16 *lr, u32 frames) { fwrite(lr, 4, frames, g_wav); g_wav_frames += frames; }

/* the game part of one frame of WinMain's loop (graphics and timing left out) */
static void game_frame(void)
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
static void game_reset(void)
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
static void draw_frame(void)
{
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

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s DATA_DIR [FRAMES] [FRAME:MASK | FROM-TO:MASK ...]\n", argv[0]); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (!load_image()) { fprintf(stderr, "out of memory\n"); return 1; }
    static char root[1024]; snprintf(root, sizeof root, "%s", argv[1]);
    g_pc_root = dirname(root);
    int frames = argc > 2 ? atoi(argv[2]) : 600;
    int npress = 0, press_at[32]; u32 press_mask[32];
    int nhold = 0, hold_from[32], hold_to[32]; u32 hold_mask[32];
    for (int i = 3; i < argc; i++) {
        if (nhold < 32 && sscanf(argv[i], "%d-%d:%x", &hold_from[nhold], &hold_to[nhold], &hold_mask[nhold]) == 3) nhold++;
        else if (npress < 32 && sscanf(argv[i], "%d:%x", &press_at[npress], &press_mask[npress]) == 2) npress++;
    }
    int trace = getenv("DC2_TRACE") != NULL;
    const Renderer *rend = NULL;
#ifdef DC2_GL
    rend = RenderGl_Open(640, 480, getenv("DC2_SCALE") ? atoi(getenv("DC2_SCALE")) : 1);
    if (!rend) return 1;
    g_rend = rend;
    int quit = 0;
    if (getenv("DC2_STATS")) RenderGl_Stats()->on_present = gl_stats;
    audio_open();
    int log_from = getenv("DC2_DRAWLOG") ? atoi(getenv("DC2_DRAWLOG")) : -1;
#endif
    if (getenv("DC2_WAV") && (g_wav = fopen(getenv("DC2_WAV"), "wb"))) { wav_header(); PcAudio_Init(44100, wav_sink); }
    PcDraw_InitStandalone(rend, 640, 480);

    U32(A_G_PTR) = A_G_STATIC;
    game_reset();
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
        u32 held = 0;
        for (int k = 0; k < npress; k++) if (press_at[k] == f) { held |= press_mask[k]; printf("frame %4d: press %X\n", f, press_mask[k]); }
        for (int k = 0; k < nhold; k++) if (f >= hold_from[k] && f <= hold_to[k]) {
            held |= hold_mask[k];
            if (f == hold_from[k]) printf("frame %4d: hold %X until %d\n", f, hold_mask[k], hold_to[k]);
        }
#ifdef DC2_GL
        u32 t0 = SDL_GetTicks();
        held |= RenderGl_Pad(&quit);
        RenderGl_Stats()->log = log_from >= 0 && f >= log_from && f < log_from + (getenv("DC2_DRAWLOG_N") ? atoi(getenv("DC2_DRAWLOG_N")) : 2);
        if (quit) break;
#endif
        g_pad_raw[0] = (u16)held;                   /* read by Input_Frame (0x478BF0) */
        game_frame();
        PcAudio_Frame(U32(0x87DE00));                  /* the frame's sound */
        draw_frame();
#ifdef DC2_GL
        u32 want = 1000 * (U32(0x87DE00) ? U32(0x87DE00) : 1) / 60, dt = SDL_GetTicks() - t0;
        if (!getenv("DC2_FAST") && dt < want) SDL_Delay(want - dt);
#endif
        u32 mode = U8(0x74D608) | U8(0x74D60A) << 8;
        if (mode != last_mode) { printf("frame %4d: root mode %u step %u\n", f, mode & 0xFF, mode >> 8); last_mode = mode; }
        static u32 last_title = 0xFFFFFFFF;
        u32 title = U16(G() + 0x76E) | U16(G() + 0x770) << 8 | U16(G() + 0x772) << 16;
        if (U16(G() + 0x76C) && title != last_title) {        /* once the title runs */
            printf("frame %4d: title screen %u step %u/%u cursor %d\n", f, title & 0xFF, (title >> 8) & 0xFF, title >> 16, S8(0x759EF2));
            last_title = title;
        }
        static u32 last_fade = 0xFFFFFFFF;                  /* the screen's fade: kind, level (0 clear .. 0xFF0000 black) */
        u32 fade = U8(G() + 0x814) | (U32(G() + 0x810) >> 16) << 8;
        if (getenv("DC2_TRACE") && fade != last_fade && (fade & 0xFF) != 0xFF) {
            printf("frame %4d: fade %d level %u\n", f, (s8)fade, fade >> 8);
            last_fade = fade;
        }
        static u32 last_masks = 0;                          /* the background masks' vertices */
        if (getenv("DC2_TRACE") && U32(0x864494) != last_masks) {
            printf("frame %4d: masks %u tiles\n", f, U32(0x864494) / 6);
            last_masks = U32(0x864494);
        }
        static u32 last_game = 0xFFFFFFFF;
        u32 gm = U16(G() + 0x664) | U8(G() + 0x1091) << 16 | U8(G() + 0x1090) << 8;
        if (gm != last_game && U16(G() + 0x664)) {
            printf("frame %4d: game mode %u, room ST%X%02X\n", f, gm & 0xFF, gm >> 16, (gm >> 8) & 0xFF);
            last_game = gm;
        }
        if (trace && f % 50 == 0 && U16(G() + 0x664) == 3 && U8(0x87F940)) {
            gptr pl = U32(G() + 0x940);
            printf("frame %4d: player %d,%d,%d yaw %03X state %X level %u health %d\n", f, S16(pl + 0x40), S16(pl + 0x42), S16(pl + 0x44),
                   U16(pl + 0x4A), U32(pl + 0x54), U16(pl + 0x5A), S16(pl + 0x108));
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
    if (g_wav) { wav_header(); fclose(g_wav); }
    return 0;
}
