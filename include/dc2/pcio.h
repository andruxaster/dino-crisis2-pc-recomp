/* Files and heap as the PC port provides them (0x4C03F0.. and the CRT).
 * In the hook build and the harness these call the original functions; the
 * standalone builds implement them on the host (src/game/pcio.c): files are
 * read from g_pc_root, memory comes from a heap inside the emulated image so
 * that the game can keep 32-bit addresses to it. */
#ifndef DC2_PCIO_H
#define DC2_PCIO_H
#include "types.h"

s32  DC2CALL Pc_Open(gptr path, u32 a, u32 b);          /* 0x4C03F0: fd or -1 */
s32  DC2CALL Pc_Read(s32 fd, gptr buf, u32 n);          /* 0x4C04E0 */
u32  DC2CALL Pc_Size(s32 fd);                           /* 0x4C0540 */
s32  DC2CALL Pc_Seek(s32 fd, s32 off, u32 whence);      /* 0x4C0580 */
s32  DC2CALL Pc_SeekSet(s32 fd, s32 off);               /* 0x4C0560: from the start */
void DC2CALL Pc_Close(s32 fd);                          /* 0x4C05A0 */
s32  DC2CALL Pc_Creat(gptr path);                       /* 0x4C0490: PCcreat, fopen wb (-1 only when 16 are open) */
s32  DC2CALL Pc_Write(s32 fd, gptr buf, u32 n);         /* 0x4C0510: PCwrite */
gptr DC2CALL Crt_malloc(u32 n);                         /* 0x4DD711 */
void DC2CALL Crt_free(gptr p);                          /* 0x4DD628 */
void DC2CALL Crt_sprintf1(gptr buf, gptr fmt, u32 a);   /* 0x4DD5D6: sprintf with one argument */
void DC2CALL Crt_sprintf3(gptr buf, gptr fmt, u32 a, u32 b, u32 c);   /* ... with three numbers */

/* DirectSound buffers and the MP3 streams (0x4BF720..): in the standalone
 * builds a mixer (src/pc/audio.c) that the port pulls samples from */
u32  DC2CALL Snd_CreateBuffer(gptr wave, u32 voice);   /* 0x4BF720: handle (0 for the voice) or -1 */
void DC2CALL Snd_Play(u32 h, u32 a, u32 loop);          /* 0x4BF970 */
void DC2CALL Snd_SetVolume(u32 h, s32 vol);             /* 0x4BFA20 */
u32  DC2CALL Snd_IsPlaying(u32 h);                      /* 0x4BFAA0 */
void DC2CALL Snd_Stop(u32 h);                           /* 0x4BFAE0 */
void DC2CALL Snd_Free(u32 h);                           /* 0x4BFBD0 */
void DC2CALL Strm_SetVolume(u32 h, s32 vol);            /* 0x4BFF20 */
void DC2CALL Strm_Play(u32 h);                          /* 0x4BFEE0 */
void DC2CALL Strm_Stop(u32 h);                          /* 0x4BFFA0 */
void DC2CALL Snd_SetPan(u32 h, u32 pan);                /* 0x4BFA60 */
void DC2CALL Strm_Close(u32 h);                         /* 0x4C0050 */
void DC2CALL Snd_Pause(u32 h);                          /* 0x4BFB00: stopped where it is, if playing */
void DC2CALL Snd_Resume(u32 h);                         /* 0x4BFB40: on from there, if paused */
void DC2CALL Strm_Pause(u32 h);                         /* 0x4BFFD0 */
void DC2CALL Strm_Resume(u32 h);                        /* 0x4C0010 */
u32  DC2CALL Strm_Open(gptr mp3, u32 size, u32 loop);  /* 0x4BFC10: handle or -1 */
#ifndef DC2_IN_PROCESS
/* standalone: record rec (header index) of file ("MS_0001.DAT") played from
   data\BGM\ (tools/dreamcast/dc2_music.py): handle or -1 */
u32  Strm_OpenFile(const char *file, u32 rec, u32 loop);
/* small-memory builds: the voice (a wave file, game path) played from the
   file as the voice buffer, handle 0; -1 when it is not there */
u32  Snd_OpenFileVoice(const char *game_path);
/* an IMA ADPCM block (WAV format 0x11) of n bytes into up to spb frames of ch channels */
/* the mixer's lock (the Dreamcast mixes in its sound thread): held by code
   that shares data with a mixer source, such as the movie player */
void PcAudio_Lock(int on);
u32  PcAudio_Bytes(void);        /* the memory the mixer's voices hold */
u32  PcAudio_ImaBlock(const u8 *b, u32 n, u32 ch, s16 *out, u32 spb);
#endif

#ifndef DC2_IN_PROCESS
/* the mixer's output: stereo s16 at rate; PcAudio_Frame mixes the samples of
 * one frame of play (n sixtieths of a second) and hands them to the sink */
void PcAudio_Init(u32 rate, void (*sink)(const s16 *lr, u32 frames));
void PcAudio_Frame(u32 sixtieths);
void PcAudio_Mix(s16 *lr, u32 frames);
/* a source mixed in at full volume (the movie's sound): adds up to n frames
   of stereo s16 at the mixer's rate to lr (as float), returns how many */
void PcAudio_SetSource(u32 (*add)(float *lr, u32 frames));
u32  PcAudio_Rate(void);
/* "data\\NAME" -> the host's path under g_pc_root, each component matched
   without case */
int  Pc_HostPath(const char *game_path, char *out, unsigned long n);
#endif

/* movies (standalone: src/pc/movie.c, the MPEG-1 files of MOVIE decoded) */
u32  DC2CALL Movie_Open(gptr name, u32 w, u32 h);       /* 0x4BF3A0: 0 when it plays */
void DC2CALL Movie_Ctl1(void);                          /* 0x4BF650: plays */
void DC2CALL Movie_Ctl0(void);                          /* 0x4BF670: stops */
u32  DC2CALL Movie_Poll(void);                          /* 0x4BF690: nonzero when it has ended */
void DC2CALL Movie_Close(void);                         /* 0x4BF340 */
void DC2CALL Movie_Clear(u32 mode);                     /* 0x4B4F90: the screen cleared (PcDraw_Clear) */

/* the background masks as Direct3D quads (standalone: src/pc/drawot.c) */
void DC2CALL Masks_CreateVB(u32 nverts, gptr pixels);   /* 0x4BE3C0 */
void DC2CALL Masks_SetQuad(u32 i, u32 x, u32 y, u32 z); /* 0x4BE430 */
void DC2CALL Masks_BuildTexture(void);                  /* 0x4BE6E0 */
void DC2CALL Masks_Upload(u32 a);                       /* 0x4BEAC0 */
void DC2CALL Masks_ShiftDepth(u32 d);                   /* 0x4BE6A0: all masks d deeper */
u32  DC2CALL Masks_FillMarker(u32 i, gptr p);           /* 0x4BEC40: mask i into the marker p; its z */
void DC2CALL SetMaskMarker(gptr p);                     /* 0x4C06B0: code 0x88 */

/* the fog (D3D render states; standalone: src/pc/drawot.c): colour 0xRRGGBB, start and end as float bits */
void DC2CALL Fog_SetColor(u32 rgb, u32 start, u32 end);   /* 0x4B4E60 */
void DC2CALL Fog_Enable(u32 on);                          /* 0x4B4F50 */

/* the GPU layer (DirectDraw/Direct3D): the original in-process, src/pc/gpu.c
   standalone (include/dc2/gpu.h) */
void DC2CALL Gpu_AddPrim(gptr ot, gptr p);                  /* 0x4C05D0 */
void DC2CALL Gpu_DrawOT(gptr ot, gptr a, gptr b);           /* 0x4C0600: AddPrims, the chain a..b into ot */
void DC2CALL Gpu_SetSprt(gptr p);                           /* 0x4C0690 */
void DC2CALL Gpu_VramCopy(gptr r, s32 x, s32 y);          /* 0x4B4A60: the rect copied to x, y in VRAM */
void DC2CALL Gpu_MoveImage(gptr r, s32 x, s32 y);         /* 0x4B4B30: MoveImage */
void DC2CALL Gpu_SetTile(gptr p);                           /* 0x4C06A0 */
void DC2CALL Gpu_SetSemiTrans(gptr p, u32 on);              /* 0x4C06C0 */
void DC2CALL Gpu_SetDrawArea(gptr p, gptr r);               /* 0x4C06E0 */
void DC2CALL Gpu_SetDrawMode(gptr p, u32 dfe, u32 dtd, u32 tpage, u32 tw); /* 0x4C06F0 */
void DC2CALL Gpu_SetDrawVramOp(gptr p, gptr rect, u32 x, u32 y);  /* 0x4C0740: code 0xEC, the rect to x, y in VRAM when drawn */
u32  DC2CALL Gpu_GetTPage(u32 tp, u32 abr, u32 x, u32 y);   /* 0x4C08A0 */
void DC2CALL Gpu_LoadImage(gptr rect, gptr p);              /* 0x4B4D20 */
void DC2CALL Gpu_LoadImageIfChanged(gptr rect, gptr p);     /* 0x4B4DB0 */
void DC2CALL Gpu_StoreImage(gptr rect, gptr p);             /* 0x4B49F0 */
/* the background masks' vertex buffer and textures made (masks module, 0x4BEAC0) */
/* a JPEG (size bytes) to dst; mode 2: 15-bit RGB, width*2 bytes a row */
void DC2CALL JPEG_Decode(gptr dst, gptr src, u32 size, u32 mode); /* 0x4D2990 */

/* the pads (DirectInput): raw buttons into 0x87DC42 + 0x24 * pad */
void DC2CALL Input_ReadPad(u32 pad);                    /* 0x4BEF40 */
u32  DC2CALL Input_ReadKey(void);                       /* 0x4BF180: the first key down that has a name (0: none) */
void DC2CALL Display_Apply(void);                       /* 0x46FC60: the display re-made for 0x723CA0.. (Win32 glue) */
#ifndef DC2_IN_PROCESS
extern u16 g_pad_raw[2];          /* standalone: what the driver presses */
#endif

/* cooperative tasks: give up the CPU for n frames (0x4C29A9, task.c) */
void DC2CALL Task_Sleep(u32 n);

#ifndef DC2_IN_PROCESS
extern const char *g_pc_root;     /* folder that holds "data" (default ".") */
extern const char *g_pc_save_root; /* folder of the saves (SAVExx.DAT; NULL: g_pc_root) */
/* called when a file the game made (Pc_Creat) is closed: its host path and game name */
extern void (*g_pc_save_written)(const char *host, const char *name);
#ifndef HEAP_SIZE
#define HEAP_SIZE 0x2000000u      /* after the image and the TMP area; music streams are ~8 MB (DC2_LOWMEM: ~2 MB used) */
#endif
u32 Pc_HeapPeak(void);            /* the most of the heap used so far (bytes) */
/* fread that tries again from the same place after a failed read (the
   Dreamcast's disc can fail a request: KallistiOS "cdrom_stream_request:
   Previous DMA request is in progress"); n read, or less only at the end of
   the file or after the tries. Takes a FILE * (void * here: no stdio.h) */
size_t Host_Read(void *p, size_t n, void *file);
/* a file read from start to end as it plays (music, speech, movies): on a
   real drive (DC2_LOWMEM builds, the Dreamcast) given a read buffer of
   about `buf` bytes, 32-byte aligned, so the disc is read in large pieces
   seldom rather than a block at a time (each read of another file costs the
   drive a seek); elsewhere a plain fopen. Closed with Host_CloseStream. */
void *Host_OpenStream(const char *path, size_t buf);
void Host_CloseStream(void *file);
/* the same for a sound's file (music, speech): the port's own reader first
   (the Dreamcast keeps the read-ahead in sound RAM, g_audio_stream_hook),
   else Host_OpenStream with a smaller main-RAM buffer there */
void *Host_OpenAudioStream(const char *path, size_t buf);
extern void *(*g_audio_stream_hook)(const char *path, size_t buf);
u32 Host_ReadFailures(void);      /* the reads that needed another try (the log) */
#endif
#endif
