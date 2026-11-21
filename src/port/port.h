/* The standalone builds' shared main-loop pieces (frame.c). */
#ifndef DC2_PORT_H
#define DC2_PORT_H
void Port_GameFrame(void);     /* the game part of one frame of WinMain's loop */
void Port_GameReset(void);     /* the start of a run (0x470980) */
void Port_DrawFrame(void);     /* the graphics part of the loop (0x47053E) */
#endif
