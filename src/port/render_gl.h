/* The OpenGL / SDL2 renderer (render_gl.c). */
#ifndef DC2_RENDER_GL_H
#define DC2_RENDER_GL_H
#include "dc2/render.h"

typedef struct RenderGlStats RenderGlStats;
struct RenderGlStats {
    u32 frames, draws, verts, uploads;           /* draws, verts: in the frame so far */
    int log;                                     /* each draw printed */
    void (*on_present)(RenderGlStats *);         /* called before each frame is shown */
};

/* a window of w x h times scale; NULL if it cannot be opened */
const Renderer *RenderGl_Open(int w, int h, int scale, int fullscreen);
RenderGlStats *RenderGl_Stats(void);
/* the window w x h, or full screen on the desktop's mode (the options' display page) */
void RenderGl_SetMode(int w, int h, int fullscreen);
/* events handled, the pad's buttons from the keyboard (quit set on close or Escape) */
u16 RenderGl_Pad(int *quit);
#endif
