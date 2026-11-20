/* See include/dc2/pcio.h. */
#include "dc2/pcio.h"
#include "dc2/game.h"
#include "dc2/gte.h"
#include "dc2/gpu.h"
#include "dc2/pcdraw.h"

#ifdef DC2_IN_PROCESS
#define ORIG(addr, type) ((type)(uintptr_t)(addr))
s32  DC2CALL Pc_Open(gptr p, u32 a, u32 b)      { return ORIG(0x4C03F0, s32 (DC2CALL *)(gptr, u32, u32))(p, a, b); }
s32  DC2CALL Pc_Read(s32 fd, gptr buf, u32 n)   { return ORIG(0x4C04E0, s32 (DC2CALL *)(s32, gptr, u32))(fd, buf, n); }
u32  DC2CALL Pc_Size(s32 fd)                    { return ORIG(0x4C0540, u32 (DC2CALL *)(s32))(fd); }
s32  DC2CALL Pc_Seek(s32 fd, s32 off, u32 w)    { return ORIG(0x4C0580, s32 (DC2CALL *)(s32, s32, u32))(fd, off, w); }
s32  DC2CALL Pc_SeekSet(s32 fd, s32 off)         { return ORIG(0x4C0560, s32 (DC2CALL *)(s32, s32))(fd, off); }
void DC2CALL Pc_Close(s32 fd)                   { ORIG(0x4C05A0, void (DC2CALL *)(s32))(fd); }
s32  DC2CALL Pc_Creat(gptr p)                    { return ORIG(0x4C0490, s32 (DC2CALL *)(gptr))(p); }
s32  DC2CALL Pc_Write(s32 fd, gptr buf, u32 n)  { return ORIG(0x4C0510, s32 (DC2CALL *)(s32, gptr, u32))(fd, buf, n); }
gptr DC2CALL Crt_malloc(u32 n)                  { return ORIG(0x4DD711, gptr (DC2CALL *)(u32))(n); }
void DC2CALL Crt_free(gptr p)                   { ORIG(0x4DD628, void (DC2CALL *)(gptr))(p); }
void DC2CALL Task_Sleep(u32 n)                  { ORIG(0x4C29A9, void (DC2CALL *)(u32))(n); }
void DC2CALL Crt_sprintf1(gptr b, gptr f, u32 a) { ORIG(0x4DD5D6, int (__cdecl *)(gptr, gptr, u32))(b, f, a); }
void DC2CALL Crt_sprintf3(gptr b, gptr f, u32 x, u32 y, u32 z) { ORIG(0x4DD5D6, int (__cdecl *)(gptr, gptr, u32, u32, u32))(b, f, x, y, z); }
u32  DC2CALL Snd_CreateBuffer(gptr w, u32 v)    { return ORIG(0x4BF720, u32 (DC2CALL *)(gptr, u32))(w, v); }
void DC2CALL Snd_Play(u32 h, u32 a, u32 l)      { ORIG(0x4BF970, void (DC2CALL *)(u32, u32, u32))(h, a, l); }
void DC2CALL Snd_SetVolume(u32 h, s32 v)        { ORIG(0x4BFA20, void (DC2CALL *)(u32, s32))(h, v); }
u32  DC2CALL Snd_IsPlaying(u32 h)               { return ORIG(0x4BFAA0, u32 (DC2CALL *)(u32))(h); }
void DC2CALL Snd_Stop(u32 h)                    { ORIG(0x4BFAE0, void (DC2CALL *)(u32))(h); }
void DC2CALL Snd_Free(u32 h)                    { ORIG(0x4BFBD0, void (DC2CALL *)(u32))(h); }
void DC2CALL Strm_SetVolume(u32 h, s32 v)       { ORIG(0x4BFF20, void (DC2CALL *)(u32, s32))(h, v); }
void DC2CALL Strm_Play(u32 h)                   { ORIG(0x4BFEE0, void (DC2CALL *)(u32))(h); }
void DC2CALL Strm_Stop(u32 h)                   { ORIG(0x4BFFA0, void (DC2CALL *)(u32))(h); }
void DC2CALL Input_ReadPad(u32 p)               { ORIG(0x4BEF40, void (DC2CALL *)(u32))(p); }
u32  DC2CALL Input_ReadKey(void)                 { return ORIG(0x4BF180, u32 (DC2CALL *)(void))(); }
void DC2CALL Display_Apply(void)                 { ORIG(0x46FC60, void (DC2CALL *)(void))(); }
void DC2CALL Gpu_AddPrim(gptr o, gptr p)        { ORIG(0x4C05D0, void (DC2CALL *)(gptr, gptr))(o, p); }
void DC2CALL Gpu_DrawOT(gptr o, gptr a, gptr b) { ORIG(0x4C0600, void (DC2CALL *)(gptr, gptr, gptr))(o, a, b); }
void DC2CALL Gpu_VramCopy(gptr r, s32 x, s32 y)  { ORIG(0x4B4A60, void (DC2CALL *)(gptr, s32, s32))(r, x, y); }
void DC2CALL Gpu_MoveImage(gptr r, s32 x, s32 y) { ORIG(0x4B4B30, void (DC2CALL *)(gptr, s32, s32))(r, x, y); }
void DC2CALL Gpu_SetSprt(gptr p)                { ORIG(0x4C0690, void (DC2CALL *)(gptr))(p); }
void DC2CALL Gpu_SetTile(gptr p)                { ORIG(0x4C06A0, void (DC2CALL *)(gptr))(p); }
void DC2CALL Gpu_SetSemiTrans(gptr p, u32 on)   { ORIG(0x4C06C0, void (DC2CALL *)(gptr, u32))(p, on); }
void DC2CALL Gpu_SetDrawArea(gptr p, gptr r)    { ORIG(0x4C06E0, void (DC2CALL *)(gptr, gptr))(p, r); }
void DC2CALL Gpu_SetDrawMode(gptr p, u32 a, u32 b, u32 t, u32 w) { ORIG(0x4C06F0, void (DC2CALL *)(gptr, u32, u32, u32, u32))(p, a, b, t, w); }
void DC2CALL Gpu_SetDrawVramOp(gptr p, gptr r, u32 x, u32 y) { ORIG(0x4C0740, void (DC2CALL *)(gptr, gptr, u32, u32))(p, r, x, y); }
u32  DC2CALL Gpu_GetTPage(u32 t, u32 a, u32 x, u32 y) { return ORIG(0x4C08A0, u32 (DC2CALL *)(u32, u32, u32, u32))(t, a, x, y); }
void DC2CALL Gpu_LoadImage(gptr r, gptr p)      { ORIG(0x4B4D20, void (DC2CALL *)(gptr, gptr))(r, p); }
void DC2CALL Gpu_LoadImageIfChanged(gptr r, gptr p) { ORIG(0x4B4DB0, void (DC2CALL *)(gptr, gptr))(r, p); }
void DC2CALL Gpu_StoreImage(gptr r, gptr p)     { ORIG(0x4B49F0, void (DC2CALL *)(gptr, gptr))(r, p); }
void DC2CALL JPEG_Decode(gptr d, gptr s, u32 n, u32 m) { ORIG(0x4D2990, void (DC2CALL *)(gptr, gptr, u32, u32))(d, s, n, m); }
void DC2CALL Snd_SetPan(u32 h, u32 p)           { ORIG(0x4BFA60, void (DC2CALL *)(u32, u32))(h, p); }
void DC2CALL Strm_Close(u32 h)                  { ORIG(0x4C0050, void (DC2CALL *)(u32))(h); }
u32  DC2CALL Strm_Open(gptr d, u32 n, u32 l)    { return ORIG(0x4BFC10, u32 (DC2CALL *)(gptr, u32, u32))(d, n, l); }
void DC2CALL Snd_Pause(u32 h)                   { ORIG(0x4BFB00, void (DC2CALL *)(u32))(h); }
void DC2CALL Snd_Resume(u32 h)                  { ORIG(0x4BFB40, void (DC2CALL *)(u32))(h); }
void DC2CALL Strm_Pause(u32 h)                  { ORIG(0x4BFFD0, void (DC2CALL *)(u32))(h); }
void DC2CALL Strm_Resume(u32 h)                 { ORIG(0x4C0010, void (DC2CALL *)(u32))(h); }
u32  DC2CALL Movie_Open(gptr n, u32 w, u32 h)   { return ORIG(0x4BF3A0, u32 (DC2CALL *)(gptr, u32, u32))(n, w, h); }
void DC2CALL Movie_Ctl1(void)                   { ORIG(0x4BF650, void (DC2CALL *)(void))(); }
void DC2CALL Movie_Ctl0(void)                   { ORIG(0x4BF670, void (DC2CALL *)(void))(); }
u32  DC2CALL Movie_Poll(void)                   { return ORIG(0x4BF690, u32 (DC2CALL *)(void))(); }
void DC2CALL Movie_Clear(u32 m)                 { ORIG(0x4B4F90, void (DC2CALL *)(u32))(m); }
void DC2CALL Movie_Close(void)                  { ORIG(0x4BF340, void (DC2CALL *)(void))(); }
void DC2CALL Masks_CreateVB(u32 n, gptr p)      { ORIG(0x4BE3C0, void (DC2CALL *)(u32, gptr))(n, p); }
void DC2CALL Masks_SetQuad(u32 i, u32 x, u32 y, u32 z) { ORIG(0x4BE430, void (DC2CALL *)(u32, u32, u32, u32))(i, x, y, z); }
void DC2CALL Masks_BuildTexture(void)           { ORIG(0x4BE6E0, void (DC2CALL *)(void))(); }
void DC2CALL Masks_Upload(u32 a)                { ORIG(0x4BEAC0, void (DC2CALL *)(u32))(a); }
void DC2CALL Masks_ShiftDepth(u32 d)            { ORIG(0x4BE6A0, void (DC2CALL *)(u32))(d); }
u32  DC2CALL Masks_FillMarker(u32 i, gptr p)    { return ORIG(0x4BEC40, u32 (DC2CALL *)(u32, gptr))(i, p); }
void DC2CALL SetMaskMarker(gptr p)              { ORIG(0x4C06B0, void (DC2CALL *)(gptr))(p); }
void DC2CALL Fog_SetColor(u32 c, u32 a, u32 b)   { ORIG(0x4B4E60, void (DC2CALL *)(u32, u32, u32))(c, a, b); }
void DC2CALL Fog_Enable(u32 on)                 { ORIG(0x4B4F50, void (DC2CALL *)(u32))(on); }
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

const char *g_pc_root = ".";

static FILE *g_files[16];
static int resolve(const char *game_path, char *out, size_t n);

/* "data\NAME" -> root/data/NAME, each component matched without case */
int Pc_HostPath(const char *game_path, char *out, unsigned long n) { return resolve(game_path, out, n); }
static int resolve(const char *game_path, char *out, size_t n)
{
    char comp[256];
    snprintf(out, n, "%s", g_pc_root);
    for (const char *s = game_path; *s;) {
        size_t k = 0;
        while (*s && *s != '\\' && *s != '/' && k < sizeof comp - 1) comp[k++] = *s++;
        comp[k] = 0;
        while (*s == '\\' || *s == '/') s++;
        DIR *d = opendir(out);
        const char *found = comp;
        struct dirent *e;
        if (d) {
            while ((e = readdir(d))) if (!strcasecmp(e->d_name, comp)) { found = e->d_name; break; }
        }
        size_t l = strlen(out);
        snprintf(out + l, n - l, "/%s", found);
        if (d) closedir(d);
    }
    return 1;
}

s32 DC2CALL Pc_Open(gptr path, u32 a, u32 b)
{
    (void)a; (void)b;
    int fd = 0;
    while (fd < 16 && g_files[fd]) fd++;
    if (fd == 16) return -1;
    char host[1024];
    resolve((const char *)MEM(path), host, sizeof host);
    if (!(g_files[fd] = fopen(host, "rb"))) return -1;
    return fd;
}
s32 DC2CALL Pc_Read(s32 fd, gptr buf, u32 n) { return (s32)fread(MEM(buf), 1, n, g_files[fd]); }
u32 DC2CALL Pc_Size(s32 fd)
{
    long cur = ftell(g_files[fd]); fseek(g_files[fd], 0, SEEK_END);
    long n = ftell(g_files[fd]); fseek(g_files[fd], cur, SEEK_SET);
    return (u32)n;
}
s32 DC2CALL Pc_Seek(s32 fd, s32 off, u32 w) { return fseek(g_files[fd], off, (int)w); }
s32 DC2CALL Pc_SeekSet(s32 fd, s32 off) { return fseek(g_files[fd], off, SEEK_SET); }
void DC2CALL Pc_Close(s32 fd) { if (g_files[fd]) fclose(g_files[fd]); g_files[fd] = NULL; }
s32 DC2CALL Pc_Creat(gptr path)
{
    int fd = 0;
    while (fd < 16 && g_files[fd]) fd++;
    if (fd == 16) return -1;
    char host[1024];
    resolve((const char *)MEM(path), host, sizeof host);
    g_files[fd] = fopen(host, "wb");
    return fd;
}
s32 DC2CALL Pc_Write(s32 fd, gptr buf, u32 n) { return g_files[fd] ? (s32)fwrite(MEM(buf), 1, n, g_files[fd]) : 0; }

/* first-fit heap in the emulated image: blocks of {u32 size | used bit, data} */
#define HEAP_BASE (IMAGE_BASE + IMAGE_SIZE + TMP_SIZE)
static int g_heap_init;
gptr DC2CALL Crt_malloc(u32 n)
{
    if (!g_heap_init) { U32(HEAP_BASE) = HEAP_SIZE - 4; g_heap_init = 1; }
    n = (n + 15) & ~15u;
    for (gptr b = HEAP_BASE; b < HEAP_BASE + HEAP_SIZE; b += 4 + (U32(b) & ~1u)) {
        u32 sz = U32(b);
        if (sz & 1) continue;
        while (b + 4 + sz < HEAP_BASE + HEAP_SIZE && !(U32(b + 4 + sz) & 1))   /* merge free */
            sz += 4 + U32(b + 4 + sz);
        U32(b) = sz;
        if (sz < n) continue;
        if (sz >= n + 32) { U32(b + 4 + n) = sz - n - 4; sz = n; }
        U32(b) = sz | 1;
        return b + 4;
    }
    fprintf(stderr, "Crt_malloc: out of heap (%u bytes)\n", n);
    return 0;
}
void DC2CALL Crt_free(gptr p) { if (p) U32(p - 4) &= ~1u; }
void DC2CALL Crt_sprintf3(gptr b, gptr f, u32 x, u32 y, u32 z)
{
    sprintf((char *)MEM(b), (const char *)MEM(f), (int)x, (int)y, (int)z);
}

void DC2CALL Crt_sprintf1(gptr b, gptr f, u32 a)
{
    if (strstr((const char *)MEM(f), "%s")) sprintf((char *)MEM(b), (const char *)MEM(f), (const char *)MEM(a));
    else sprintf((char *)MEM(b), (const char *)MEM(f), (int)a);
}

/* the sound buffers and streams: src/pc/audio.c */
u16 g_pad_raw[2];
void DC2CALL Input_ReadPad(u32 p) { if (p < 2) U16(0x87DC42 + p * 0x24) = g_pad_raw[p]; }
u32 DC2CALL Input_ReadKey(void) { return 0; }   /* (no keyboard to bind keys from) */
void DC2CALL Display_Apply(void) {}              /* (the window keeps its mode) */
void DC2CALL Movie_Clear(u32 m) { extern void PcDraw_Clear(u32); PcDraw_Clear(m); }
/* the masks: src/pc/drawot.c (the marker: none while no masks are built) */
void DC2CALL Masks_CreateVB(u32 n, gptr p) { PcDraw_MasksCreate(n, p); }
void DC2CALL Masks_SetQuad(u32 i, u32 x, u32 y, u32 z) { PcDraw_MasksSetQuad(i, x, y, z); }
void DC2CALL Masks_BuildTexture(void) { PcDraw_MasksBuild(); }
void DC2CALL Masks_Upload(u32 a) { PcDraw_MasksUpload(a); }
void DC2CALL Masks_ShiftDepth(u32 d) { PcDraw_MasksShift(d); }
u32 DC2CALL Masks_FillMarker(u32 i, gptr p)
{
    if (!U32(0x864490)) { U32(p + 8) = i; U16(p + 0xE) = 0; U16(p + 0xC) = 0x7FFF; return 0; }
    return PcDraw_MasksMarker(i, p);
}
void DC2CALL SetMaskMarker(gptr p) { PcGpu_SetMaskMarker(p); }
void DC2CALL Fog_SetColor(u32 c, u32 a, u32 b) { PcDraw_SetFarColor(c, a, b); }
void DC2CALL Fog_Enable(u32 on) { PcDraw_SetFogEnable(on); }
void DC2CALL Gpu_AddPrim(gptr o, gptr p) { PcGpu_AddPrim(o, p); }
void DC2CALL Gpu_DrawOT(gptr o, gptr a, gptr b) { PcGpu_AddPrims(o, a, b); }
void DC2CALL Gpu_VramCopy(gptr r, s32 x, s32 y) { PcDraw_VramCopy(r, x, y); }
void DC2CALL Gpu_MoveImage(gptr r, s32 x, s32 y) { PcDraw_MoveImage(r, x, y); }
void DC2CALL Gpu_SetSprt(gptr p) { PcGpu_SetSprt(p); }
void DC2CALL Gpu_SetTile(gptr p) { PcGpu_SetTile(p); }
void DC2CALL Gpu_SetSemiTrans(gptr p, u32 on) { PcGpu_SetSemiTrans(p, on); }
void DC2CALL Gpu_SetDrawArea(gptr p, gptr r) { PcGpu_SetDrawArea(p, r); }
void DC2CALL Gpu_SetDrawMode(gptr p, u32 a, u32 b, u32 t, u32 w) { PcGpu_SetDrawMode(p, a, b, t, w); }
void DC2CALL Gpu_SetDrawVramOp(gptr p, gptr r, u32 x, u32 y) { PcGpu_SetDrawVramOp(p, r, x, y); }
u32  DC2CALL Gpu_GetTPage(u32 t, u32 a, u32 x, u32 y) { return PcGpu_GetTPage(t, a, x, y); }
void DC2CALL Gpu_LoadImage(gptr r, gptr p) { PcGpu_LoadImage(r, p); }
void DC2CALL Gpu_LoadImageIfChanged(gptr r, gptr p) { PcGpu_LoadImageIfChanged(r, p); }
void DC2CALL Gpu_StoreImage(gptr r, gptr p) { PcGpu_StoreImage(r, p); }
void DC2CALL JPEG_Decode(gptr d, gptr s, u32 n, u32 m) { PcJpeg_Decode(d, s, n, m); }
#endif
