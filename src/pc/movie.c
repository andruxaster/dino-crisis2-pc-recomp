/* The PC port's movies (0x4BF340..0x4BF6D0: a DirectShow multimedia stream
 * drawn into a DirectDraw surface) for the standalone builds. MOVIE\*.DAT
 * are MPEG-1 system streams: 320 x 160 or 320 x 240 video at 30 frames a
 * second (pl_mpeg) and MPEG audio layer I or II at 44.1 kHz (minimp3).
 *
 * Movie_Open (name, x, y) finds the file (the game's "movie\NAME.dat" under
 * g_pc_root, without case): 0 when it plays, 1 when it is missing (the game
 * then skips the movie). Movie_Ctl1 runs it, Movie_Ctl0 stops it.
 * Movie_Poll, once a frame of the game: the movie's clock moves on by the
 * frame's length (0x87DE00 sixtieths of a second, which Movie_Start sets
 * for the movie), the frame due is drawn into the back buffer at (x, y) of
 * the 320 x 240 screen, scaled like the original's blit (0x4BD530); 1 once
 * the video has ended (the movie closed). The sound goes to the mixer as a
 * source, pulled with the same clock (PcAudio_Frame after each frame).
 * DC2_NOMOVIES=1: every movie missing.
 * The Dreamcast build (DC2_MOVIE_AVI, Makefile.dreamcast only) plays
 * MOVIE\NAME.AVI instead (Cinepak, made by tools/dreamcast/dc2_movies.sh;
 * movie_avi.c): 16-bit frames, the same clock. The PC builds play the .DAT. */
#ifndef DC2_IN_PROCESS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dc2/types.h"
#include "dc2/pcio.h"
#include "dc2/render.h"
#define PL_MPEG_IMPLEMENTATION
#define PLM_NO_STDIO
#include "../../third_party/pl_mpeg/pl_mpeg.h"
#include "../../third_party/minimp3/minimp3.h"
#ifdef DC2_MOVIE_AVI
#include "movie_avi.h"
#endif

#define MOVIE_TEX_ID 0x7FFFFF00u        /* the frame's texture (apart from the PC layer's objects) */

static struct {
    FILE *f;
    plm_buffer_t *in, *vbuf;
    plm_demux_t *dmx;
    plm_video_t *vid;
    plm_frame_t *next;                  /* decoded, not yet due */
    int running, video_end;
    double clock, t0;                   /* seconds since the start; the first frame's time */
    u8 *bgra; u32 w, h, version;
    s32 x, y;
    /* sound: the stream's bytes, decoded frames */
    u8 *abuf; size_t alen, acap, apos;
    mp3dec_t mp3;
    s16 pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    u32 pcm_n, pcm_ch, pcm_hz;
    double pcm_pos;
    int avi;                            /* a Cinepak AVI (movie_avi.c) */
    u32 avi_frame;                      /* frames shown */
} M;

#ifdef DC2_MOVIE_AVI
static u32 avi_sound(float *lr, u32 frames) { return M.running ? Avi_Sound(lr, frames, PcAudio_Rate()) : 0; }

/* NAME.AVI for the game's "movie\NAME.dat"; 1 when it opened */
static int open_avi(const char *game_path)
{
    char p[256], host[1024];
    snprintf(p, sizeof p, "%s", game_path);
    char *dot = strrchr(p, '.');
    if (!dot || strlen(dot) != 4) return 0;
    strcpy(dot, ".AVI");
    Pc_HostPath(p, host, sizeof host);
    if (!Avi_Open(host)) return 0;
    M.avi = 1;
    M.w = Avi_Width(); M.h = Avi_Height();
    PcAudio_SetSource(avi_sound);
    return 1;
}
#endif

static void in_load(plm_buffer_t *b, void *user)
{
    (void)user;
    if (!M.f) { plm_buffer_signal_end(b); return; }
    u8 chunk[0x4000];
    size_t n = fread(chunk, 1, sizeof chunk, M.f);
    if (n) plm_buffer_write(b, chunk, n);
    if (n < sizeof chunk) plm_buffer_signal_end(b);
}

/* the next packet of the file to its stream; 0 at the end */
static int pump(void)
{
    plm_packet_t *p = plm_demux_decode(M.dmx);
    if (!p) {
        if (plm_demux_has_ended(M.dmx)) { plm_buffer_signal_end(M.vbuf); return 0; }
        return 1;
    }
    if (p->type == PLM_DEMUX_PACKET_VIDEO_1) plm_buffer_write(M.vbuf, p->data, p->length);
    else if (p->type == PLM_DEMUX_PACKET_AUDIO_1) {
        if (M.apos > 0x10000) {                     /* what was decoded dropped */
            memmove(M.abuf, M.abuf + M.apos, M.alen - M.apos); M.alen -= M.apos; M.apos = 0;
        }
        if (M.alen + p->length > M.acap) { M.acap = (M.alen + p->length) * 2; M.abuf = realloc(M.abuf, M.acap); }
        memcpy(M.abuf + M.alen, p->data, p->length); M.alen += p->length;
    }
    return 1;
}

static void vbuf_load(plm_buffer_t *b, void *user)
{
    (void)user;
    size_t before = plm_buffer_get_remaining(b);
    while (plm_buffer_get_remaining(b) == before && pump()) {}
}

/* the movie's sound added to the mix: frames at the mixer's rate */
static u32 add_sound(float *lr, u32 frames)
{
    if (!M.running || !M.dmx) return 0;
    double step = (double)(M.pcm_hz ? M.pcm_hz : 44100) / PcAudio_Rate();
    for (u32 i = 0; i < frames; i++) {
        while (M.pcm_pos >= M.pcm_n) {
            M.pcm_pos -= M.pcm_n; M.pcm_n = 0;
            while (M.alen - M.apos < 0x800 && pump()) {}
            if (M.alen == M.apos) return i;
            mp3dec_frame_info_t fi;
            int got = mp3dec_decode_frame(&M.mp3, M.abuf + M.apos, (int)(M.alen - M.apos), M.pcm, &fi);
            if (!fi.frame_bytes) { M.apos = M.alen; return i; }
            M.apos += (size_t)fi.frame_bytes;
            M.pcm_n = (u32)got; M.pcm_ch = (u32)fi.channels; M.pcm_hz = (u32)fi.hz;
            step = (double)(M.pcm_hz ? M.pcm_hz : 44100) / PcAudio_Rate();
        }
        u32 k = (u32)M.pcm_pos;
        float l = M.pcm[M.pcm_ch == 2 ? 2 * k : k], r = M.pcm[M.pcm_ch == 2 ? 2 * k + 1 : k];
        lr[2 * i] += l; lr[2 * i + 1] += r;
        M.pcm_pos += step;
    }
    return frames;
}

static void movie_close(void);
void DC2CALL Movie_Close(void)                                       /* 0x4BF340 */
{
    PcAudio_Lock(1); movie_close(); PcAudio_Lock(0);
}
static void movie_close(void)
{
    PcAudio_SetSource(0);
#ifdef DC2_MOVIE_AVI
    if (M.avi) Avi_Close();
#endif
    if (M.vid) plm_video_destroy(M.vid);
    if (M.dmx) plm_demux_destroy(M.dmx);
    if (M.in) plm_buffer_destroy(M.in);
    if (M.f) fclose(M.f);
    free(M.bgra); free(M.abuf);
    memset(&M, 0, sizeof M);
}

static u32 movie_open(gptr name, u32 x, u32 y);
u32 DC2CALL Movie_Open(gptr name, u32 x, u32 y)                      /* 0x4BF3A0 */
{
    PcAudio_Lock(1); u32 r = movie_open(name, x, y); PcAudio_Lock(0);
    return r;
}
static u32 movie_open(gptr name, u32 x, u32 y)
{
    Movie_Close();
    if (getenv("DC2_NOMOVIES")) return 1;
#ifdef DC2_MOVIE_AVI
    if (open_avi((const char *)MEM(name))) { M.x = (s32)x; M.y = (s32)y; return 0; }
    return 1;                           /* (the Dreamcast plays only the converted movies) */
#endif
    char host[1024];
    Pc_HostPath((const char *)MEM(name), host, sizeof host);
    if (!(M.f = fopen(host, "rb"))) return 1;
    M.in = plm_buffer_create_with_capacity(0x10000);
    plm_buffer_set_load_callback(M.in, in_load, 0);
    M.dmx = plm_demux_create(M.in, 0);
    M.vbuf = plm_buffer_create_for_appending(0x20000);
    plm_buffer_set_load_callback(M.vbuf, vbuf_load, 0);
    M.vid = plm_video_create_with_buffer(M.vbuf, 1);
    mp3dec_init(&M.mp3);
    if (!plm_video_has_header(M.vid)) { Movie_Close(); return 8; }
    M.w = (u32)plm_video_get_width(M.vid); M.h = (u32)plm_video_get_height(M.vid);
    M.bgra = malloc((size_t)M.w * M.h * 4);
    memset(M.bgra, 0, (size_t)M.w * M.h * 4);
    for (u32 i = 0; i < M.w * M.h; i++) M.bgra[4 * i + 3] = 0xFF;
    M.x = (s32)x; M.y = (s32)y;
    M.t0 = -1;
    PcAudio_SetSource(add_sound);
    return 0;
}

void DC2CALL Movie_Ctl1(void) { if (M.vid || M.avi) M.running = 1; } /* 0x4BF650 */
void DC2CALL Movie_Ctl0(void) { M.running = 0; }                    /* 0x4BF670 */

#ifdef DC2_MOVIE_AVI
static u32 poll_avi(void)
{
    if (M.running) {
        double ft = Avi_FrameTime();
        while ((double)M.avi_frame * ft <= M.clock + 1e-6) {
            if (!Avi_NextFrame()) { Movie_Close(); return 1; }
            M.avi_frame++; M.version++;
            if (M.avi_frame == Avi_Frames()) break;
        }
        /* the last frame drawn: ended now, as the MPEG player does */
        if (Avi_Frames() && M.avi_frame >= Avi_Frames()) { Movie_Close(); return 1; }
        M.clock += (double)(U32(0x87DE00) ? U32(0x87DE00) : 1) / 60.0;
    }
    if (M.version) {
        float sx = F32(0x8644BC), sy = F32(0x8644B8);
        PcSoft_DrawImage(MOVIE_TEX_ID, M.version, Avi_Pixels(), M.w, M.h, 2, (s32)((float)M.x * sx), (s32)((float)M.y * sy),
                         (s32)((float)(M.x + (s32)M.w) * sx), (s32)((float)(M.y + (s32)M.h) * sy));
    }
    return 0;
}
#endif

static u32 movie_poll(void);
u32 DC2CALL Movie_Poll(void)                                         /* 0x4BF690 */
{
    PcAudio_Lock(1); u32 r = movie_poll(); PcAudio_Lock(0);
    return r;
}
static u32 movie_poll(void)
{
#ifdef DC2_MOVIE_AVI
    if (M.avi) return poll_avi();
#endif
    if (!M.vid) return 1;
    if (M.running) {
        if (!M.next && !M.video_end && !(M.next = plm_video_decode(M.vid))) M.video_end = 1;
        if (M.next && M.t0 < 0) M.t0 = M.next->time;
        while (M.next && M.next->time - M.t0 <= M.clock + 1e-6) {
            plm_frame_to_bgra(M.next, M.bgra, (int)M.w * 4);
            M.version++;
            if (!(M.next = plm_video_decode(M.vid))) M.video_end = 1;
        }
        if (M.video_end && !M.next) { Movie_Close(); return 1; }
        M.clock += (double)(U32(0x87DE00) ? U32(0x87DE00) : 1) / 60.0;
    }
    if (M.version) {                    /* the frame, scaled like the screen (0x4BD530) */
        float sx = *(float *)MEM(0x8644BC), sy = *(float *)MEM(0x8644B8);
        PcSoft_DrawImage(MOVIE_TEX_ID, M.version, M.bgra, M.w, M.h, 4, (s32)((float)M.x * sx), (s32)((float)M.y * sy),
                         (s32)((float)(M.x + (s32)M.w) * sx), (s32)((float)(M.y + (s32)M.h) * sy));
    }
    return 0;
}
#endif
