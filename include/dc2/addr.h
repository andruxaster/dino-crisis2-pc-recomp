/* Addresses of globals in Dino2.exe (see dc2_labels.csv / dc2_game_flow.h). */
#ifndef DC2_ADDR_H
#define DC2_ADDR_H

#define A_G_PTR          0x87DDF8u   /* u32 -> game work struct G (static 0x87DE20) */
#define A_G_STATIC       0x87DE20u
#define A_FLAG_GROUPS    0x723C40u   /* 12 x {u32 *bits, u32 count} */
#define A_FLAG_CLR0      0x87DC94u   /* cleared by Flag_Set(1, 0, 1) */
#define A_FLAG_CLR1      0x87DC08u
#define A_FLAG_CLR2      0x87DC00u
#define A_FLAG_CLR3      0x87DDFCu
#define A_ROOM_BASE_IDX  0x70346Cu   /* u8 per area: first global room index */

/* room script VM */
#define A_SCR_THREADS    0x87FC20u   /* u32 -> 10 threads x 0xA4 */
#define A_SCR_GLOBALS    0x87FC40u   /* u32 -> global table (value type 6) */
#define A_SCR_OPTABLE    0x738C40u   /* 96 handler addresses */
#define SCR_NTHREADS     10
#define SCR_THREAD_SIZE  0xA4

/* G offsets */
#define G_SCRIPT         0xA54       /* u32 -> routine table of the room script */

/* thread layout */
#define T_PC     0x00
#define T_STACK  0x80                /* initial sp, stack grows down */
#define T_SP     0x84
#define T_REG    0x88                /* r0..r3 */
#define T_WAIT   0x98
#define T_STATE  0x9B                /* 1 run, 2 wait, bit7 paused */
#define T_COND   0x9C
#define T_DEPTH  0x9D
#define T_SEL0   0x9E
#define T_SEL1   0x9F
#define T_A0     0xA0

#endif
