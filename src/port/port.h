/* The standalone builds' shared main-loop pieces (frame.c). */
#ifndef DC2_PORT_H
#define DC2_PORT_H
void Port_GameFrame(void);     /* the game part of one frame of WinMain's loop */
void Port_GameReset(void);     /* the start of a run (0x470980) */
void Port_DrawFrame(void);     /* the graphics part of the loop (0x47053E) */
/* the options' display page (Opt_Display 0x4776D0) shows what the PC
   version's DirectDraw enumeration left at 0x75DEFC / 0x75BE70: here one
   device, name, and the sizes w x h (n of them, 640 x 480 first) in a window
   (can_window) and full screen; the current choice at 0x723CA0.. from
   scale and fullscreen. A confirmed change calls apply (may be NULL), which
   reads 0x723CA4 (1: full screen) and 0x723CB0 / 0x723CB4 (the size). */
void Port_DisplayInit(const char *name, int can_window, const u16 (*sizes)[2], int n, int cur, int fullscreen, void (*apply)(void));
#endif
