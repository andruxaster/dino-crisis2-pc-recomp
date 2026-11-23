/* The standalone builds' shared main-loop pieces (frame.c). */
#ifndef DC2_PORT_H
#define DC2_PORT_H
#include "dc2/types.h"
void Port_GameFrame(void);     /* the game part of one frame of WinMain's loop */
void Port_GameReset(void);     /* the start of a run (0x470980) */
void Port_DrawFrame(void);     /* the graphics part of the loop (0x47053E) */
void Port_DumpState(void);     /* the player, script threads, objects and tasks, printed */
/* once a frame: in game mode 3, nothing of the game's state changing for
   STUCK_FRAMES frames while the pad is pressed is reported with the dump */
void Port_StuckCheck(u32 frame, u16 pad);
#define STUCK_FRAMES 600
#endif
