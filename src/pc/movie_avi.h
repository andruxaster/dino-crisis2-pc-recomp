/* Cinepak AVI movies (movie_avi.c), played by movie.c when MOVIE\NAME.AVI is there. */
#ifndef DC2_MOVIE_AVI_H
#define DC2_MOVIE_AVI_H
#include "dc2/types.h"

int Avi_Open(const char *path);          /* 1: a Cinepak AVI it can play */
void Avi_Close(void);
u32 Avi_Width(void);
u32 Avi_Height(void);
double Avi_FrameTime(void);              /* seconds a frame */
u32 Avi_Frames(void);                    /* frames in the movie (0: not given) */
int Avi_NextFrame(void);                 /* the next frame decoded into Avi_Pixels(); 0 at the end */
const u16 *Avi_Pixels(void);             /* ARGB1555, width x height */
u32 Avi_Sound(float *lr, u32 frames, u32 rate);   /* the soundtrack added in; frames given (fewer at the end) */
#endif
