/* Reimplemented game functions. Each one keeps the name, arguments and
 * behaviour of the original at the address given in the comment; the hook
 * table (src/hook/hooks.c) maps those addresses to these functions. */
#ifndef DC2_GAME_H
#define DC2_GAME_H
#include "types.h"
#include "addr.h"

static inline gptr G(void) { return U32(A_G_PTR); }

/* lzss.c */
void DC2CALL Lzss_Decode(gptr dst, gptr src, s32 srclen);              /* 0x4041F0 */

/* flag.c */
void DC2CALL Flag_Set(u32 group, u32 bit, u32 on);                      /* 0x46FAA0 */
u32  DC2CALL Flag_Check(u32 group, u32 bit);                            /* 0x46FB20 */
void DC2CALL Bits_Set(gptr bits, u32 bit, u32 on);                      /* 0x46FB50 */
u32  DC2CALL Bits_Check(gptr bits, u32 bit);                            /* 0x46FB90 */

/* geom.c */
u32  DC2CALL PointInQuad(gptr pos, gptr quad);                          /* 0x48F790 */
u32  DC2CALL Room_GlobalIndex(u32 area, u32 room);                      /* 0x472210 */

/* script_vm.c -- room script virtual machine */
s32  DC2CALL Script_GetValue(gptr thr, u32 type, s32 v);                /* 0x48B8E0 */
void DC2CALL Script_SetValue(gptr thr, u32 type, s32 v, s32 x);         /* 0x48B930 */
s32  DC2CALL Script_BinOp(s32 a, s32 b, u32 op);                        /* 0x48B980 */
void DC2CALL Script_SetCond(gptr thr, s32 v);                           /* 0x48BB20 */
void DC2CALL Script_StartThread(u32 id, u32 routine);                   /* 0x48BDB0 */
void DC2CALL Script_KillThread(u32 id);                                 /* 0x48BE20 */
void DC2CALL Script_Run(void);                                          /* 0x48BC50 */
u32  DC2CALL ScrOp_Nop(gptr thr);                                       /* 0x489990 */
u32  DC2CALL ScrOp_Load(gptr thr);                                      /* 0x4873C0  01 */
u32  DC2CALL ScrOp_Store(gptr thr);                                     /* 0x487400  02 */
u32  DC2CALL ScrOp_Arith(gptr thr);                                     /* 0x487440  03 */
u32  DC2CALL ScrOp_Return(gptr thr);                                    /* 0x4874D0  04 */
u32  DC2CALL ScrOp_Push(gptr thr);                                      /* 0x487510  05 */
u32  DC2CALL ScrOp_Pop(gptr thr);                                       /* 0x487550  06 */
u32  DC2CALL ScrOp_Branch(gptr thr);                                    /* 0x487590  07 */
u32  DC2CALL ScrOp_Call(gptr thr);                                      /* 0x4875D0  08 */
u32  DC2CALL ScrOp_Wait(gptr thr);                                      /* 0x487620  10 */
u32  DC2CALL ScrOp_FlagSet(gptr thr);                                   /* 0x487B70  1C */
u32  DC2CALL ScrOp_FlagCheck(gptr thr);                                 /* 0x487BF0  1D */

/* script_obj.c -- built-ins on objects */
void DC2CALL Obj_SyncLinks(gptr obj);                                   /* 0x48FC70 */
void DC2CALL Obj_SavePos(gptr obj);                                     /* 0x48FC40 */
void DC2CALL Obj_Disable(gptr obj);                                     /* 0x48D800 */
void DC2CALL Obj_Free(gptr obj);                                        /* 0x48D890 */
void DC2CALL Obj_UpdateMatrix(gptr obj);                                /* 0x48FDE0 */
void DC2CALL Camera_LookAt(gptr target, gptr dist, gptr angles, gptr eye); /* 0x403840 */
void DC2CALL Camera_Switch(u32 cam);                                    /* 0x403A50 */
u32  DC2CALL ScrOp_ObjSelect(gptr thr);                                 /* 0x487690  12 */
u32  DC2CALL ScrOp_EffectInit(gptr thr);                                /* 0x4876C0  13 */
u32  DC2CALL ScrOp_Obj14Init(gptr thr);                                 /* 0x4877A0  14 */
u32  DC2CALL ScrOp_ThreadStart(gptr thr);                               /* 0x4878B0  15 */
u32  DC2CALL ScrOp_ThreadKill(gptr thr);                                /* 0x487900  16 */
u32  DC2CALL ScrOp_ThreadPause(gptr thr);                               /* 0x487940  17 */
u32  DC2CALL ScrOp_ThreadResume(gptr thr);                              /* 0x487970  18 */
u32  DC2CALL ScrOp_ThreadGoto(gptr thr);                                /* 0x4879B0  19 */
u32  DC2CALL ScrOp_ObjSet(gptr thr);                                    /* 0x487C50  1E */
u32  DC2CALL ScrOp_ObjGet(gptr thr);                                    /* 0x488770  1F */
u32  DC2CALL ScrOp_WaitFrame(gptr thr);                                 /* 0x4893E0  2A */
u32  DC2CALL ScrOp_Obj44Init(gptr thr);                                 /* 0x48A220  44 */
u32  DC2CALL ScrOp_ObjEnable(gptr thr);                                 /* 0x48A2C0  45 */
u32  DC2CALL ScrOp_CutsceneCamera(gptr thr);                            /* 0x48A670  4A */

/* handler used for an opcode that is not reimplemented yet: in the hook
 * build it calls the original code, elsewhere it reports and stops the thread */
typedef u32 (DC2CALL *ScrOpFn)(gptr thr);
extern ScrOpFn g_scr_ops[96];
u32 DC2CALL ScrOp_Original(gptr thr);
#endif
