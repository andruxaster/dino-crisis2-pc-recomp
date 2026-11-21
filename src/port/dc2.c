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
 * Without DATA_DIR the game's Data folder is looked for under [Assets] Path of
 * config.ini (next to the executable; relative to its folder), else in the
 * executable's own folder; FRAMES 0 runs until the window is closed (the
 * default with a window).
 *   usage: dc2 [DATA_DIR [FRAMES] [FRAME:MASK | FROM-TO:MASK ...]] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#define sigjmp_buf jmp_buf
#define sigsetjmp(b, m) setjmp(b)
#define siglongjmp longjmp
#elif defined(__linux__)
#include <unistd.h>
#include <sys/mman.h>
#endif
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
#include "port.h"
#ifdef DC2_GL
#include <SDL2/SDL.h>
#include "render_gl.h"
#endif

u8 *g_membase;

/* ---- where things are: the executable's folder, config.ini ----------------------------------- */

static char g_exe_dir[1024] = ".";

static void find_exe_dir(const char *argv0)
{
    char p[1024] = "";
#ifdef _WIN32
    GetModuleFileNameA(NULL, p, sizeof p);
#elif defined(__linux__)
    ssize_t n = readlink("/proc/self/exe", p, sizeof p - 1);
    if (n > 0) p[n] = 0; else p[0] = 0;
#endif
    if (!p[0]) snprintf(p, sizeof p, "%s", argv0);
    char *e = p + strlen(p);
    while (e > p && e[-1] != '/' && e[-1] != '\\') e--;
    if (e > p) { e[-1] = 0; snprintf(g_exe_dir, sizeof g_exe_dir, "%s", p); }
}

static int strcasecmp_(const char *a, const char *b)
{
    for (; *a && tolower((unsigned char)*a) == tolower((unsigned char)*b); a++, b++) {}
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* config.ini: [Display] Scale, FullScreen; [Assets] Path (keys without case) */
static struct { int scale, fullscreen; char path[1024]; } g_cfg = {2, 0, ""};

static void load_config(void)
{
    char fn[1100], line[1100], sec[64] = "";
    snprintf(fn, sizeof fn, "%s/config.ini", g_exe_dir);
    FILE *f = fopen(fn, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *l = line;
        while (isspace((unsigned char)*l)) l++;
        char *c = strpbrk(l, ";#\r\n"); if (c) *c = 0;
        if (*l == '[') { char *r = strchr(l, ']'); if (r) { *r = 0; snprintf(sec, sizeof sec, "%s", l + 1); } continue; }
        char *eq = strchr(l, '='); if (!eq) continue;
        *eq = 0;
        char *k = l, *v = eq + 1, *t = eq;
        while (t > k && isspace((unsigned char)t[-1])) *--t = 0;
        while (isspace((unsigned char)*v)) v++;
        for (t = v + strlen(v); t > v && isspace((unsigned char)t[-1]);) *--t = 0;
        #define IS(a, b) (!strcasecmp_(a, b))
        if (IS(sec, "Display") && IS(k, "Scale")) g_cfg.scale = atoi(v);
        else if (IS(sec, "Display") && IS(k, "FullScreen")) g_cfg.fullscreen = atoi(v);
        else if (IS(sec, "Assets") && IS(k, "Path")) snprintf(g_cfg.path, sizeof g_cfg.path, "%s", v);
        #undef IS
    }
    fclose(f);
}

static int is_abs(const char *p) { return p[0] == '/' || p[0] == '\\' || (p[0] && p[1] == ':'); }

/* the folder holding Data: [Assets] Path (from the executable's folder when
   relative), else the executable's folder */
static void default_root(char *out, size_t n)
{
    const char *parts[3] = {g_exe_dir, "/", g_cfg.path};
    if (is_abs(g_cfg.path)) parts[0] = parts[1] = "";
    else if (!g_cfg.path[0]) parts[1] = "";
    size_t l = 0;
    for (int i = 0; i < 3; i++) for (const char *c = parts[i]; *c && l + 1 < n; c++) out[l++] = *c;
    out[l] = 0;
}

/* DATA_DIR's parent: the root the game's "data\..." paths start from */
static void parent_of(char *p)
{
    size_t l = strlen(p);
    while (l > 1 && (p[l - 1] == '/' || p[l - 1] == '\\')) p[--l] = 0;
    char *e = p + l;
    while (e > p && e[-1] != '/' && e[-1] != '\\') e--;
    if (e == p) snprintf(p, 2, ".");
    else if (e - 1 == p) p[1] = 0;
    else e[-1] = 0;
}

/* the memory image: zero, the tables of src/data at their addresses */
static int load_image(void)
{
#ifdef __linux__
    if (getenv("DC2_GUARD_TEXT")) {
        /* debug: the original's code (0x400000..IMAGE_DATA) made inaccessible,
           to show that standalone code never touches it (the Dreamcast build
           leaves it out of the image) */
        size_t n = (IMAGE_SIZE + TMP_SIZE + HEAP_SIZE + 0xFFF) & ~(size_t)0xFFF;
        g_membase = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (g_membase == MAP_FAILED) return 0;
        mprotect(g_membase, IMAGE_DATA - IMAGE_BASE, PROT_NONE);
    } else
#endif
    g_membase = calloc(1, IMAGE_SIZE + TMP_SIZE + HEAP_SIZE);
    if (!g_membase) return 0;
    Data_Init();
    /* as the PC layer's input start-up (0x4BEDD7) without saved settings
       (0x866144 clear): the default keys and joystick buttons, which the
       options screen reads (Opt_KeyNames) */
    if (!U32(0x866144)) { PcIn_DefaultKeys(); PcIn_DefaultButtons(); }
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

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    find_exe_dir(argv[0]);
    load_config();
    if (!load_image()) { fprintf(stderr, "out of memory\n"); return 1; }
    static char root[1024];
    if (argc > 1) { snprintf(root, sizeof root, "%s", argv[1]); parent_of(root); }
    else default_root(root, sizeof root);
    g_pc_root = root;
    if (getenv("DC2_SAVEDIR")) g_pc_save_root = getenv("DC2_SAVEDIR");   /* the saves elsewhere (default: next to Data) */
#ifdef DC2_GL
    int frames = argc > 2 ? atoi(argv[2]) : 0;
#else
    int frames = argc > 2 ? atoi(argv[2]) : 600;
#endif
    {   /* the data must be there: CORE.DAT */
        char probe[1100];
        Pc_HostPath("data\\CORE.DAT", probe, sizeof probe);
        FILE *t = fopen(probe, "rb");
        if (!t) {
            fprintf(stderr, "Dino Crisis 2: the game's Data folder was not found (%s).\n"
                            "Put Data next to the executable, set [Assets] Path in config.ini, or give DATA_DIR.\n", probe);
#if defined(DC2_GL) && defined(_WIN32)
            char m[1300]; snprintf(m, sizeof m, "The game's Data folder was not found:\n%s\n\nPut Data next to the executable or set [Assets] Path in config.ini.", probe);
            MessageBoxA(NULL, m, "Dino Crisis 2", MB_ICONERROR);
#endif
            return 1;
        }
        fclose(t);
    }
    int npress = 0, press_at[32]; u32 press_mask[32];
    int nhold = 0, hold_from[32], hold_to[32]; u32 hold_mask[32];
    for (int i = 3; i < argc; i++) {
        if (nhold < 32 && sscanf(argv[i], "%d-%d:%x", &hold_from[nhold], &hold_to[nhold], &hold_mask[nhold]) == 3) nhold++;
        else if (npress < 32 && sscanf(argv[i], "%d:%x", &press_at[npress], &press_mask[npress]) == 2) npress++;
    }
    int trace = getenv("DC2_TRACE") != NULL;
    const Renderer *rend = NULL;
#ifdef DC2_GL
    rend = RenderGl_Open(640, 480, getenv("DC2_SCALE") ? atoi(getenv("DC2_SCALE")) : g_cfg.scale, g_cfg.fullscreen);
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
    Port_GameReset();
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
    for (f = 0; !frames || f < frames; f++) {
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
        Port_GameFrame();
        PcAudio_Frame(U32(0x87DE00));                  /* the frame's sound */
        Port_DrawFrame();
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
    if (getenv("DC2_HEAPSTAT")) printf("heap peak %u KB of %u KB, surfaces peak %u KB\n", Pc_HeapPeak() >> 10, HEAP_SIZE >> 10, PcSoft_SurfacePeak() >> 10);
    return 0;
}
