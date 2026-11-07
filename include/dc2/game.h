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

/* object.c -- generic object handler and shared object helpers */
u32  DC2CALL Latch_74B16C(u32 a);                                       /* 0x448730 */
void DC2CALL Obj_PickGroup(gptr obj);                                   /* 0x46B8F0 */
u32  DC2CALL Angle_Approach(u32 cur, u32 max, u32 target);              /* 0x47A5E0 */
u32  DC2CALL Angle_ApproachLim(u32 cur, u32 max, u32 target, u32 lim);  /* 0x47A6F0 */
void DC2CALL Head_Reset(gptr o);                                        /* 0x47A880 */
void DC2CALL Head_Update(gptr o);                                       /* 0x47A930 */
void DC2CALL Head_Relax(gptr o);                                        /* 0x47A970 */
void DC2CALL Head_LookAtObj(gptr o);                                    /* 0x47AA60 */
void DC2CALL Head_LookAtPoint(gptr o);                                  /* 0x47AB40 */
void DC2CALL Head_Turn(gptr o);                                         /* 0x47ABA0 */
u32  DC2CALL Dist2_XZ(gptr a, gptr b);                                  /* 0x487300 */
void DC2CALL Obj_MoveLocal(gptr o, gptr v);                             /* 0x48F3A0 */
u32  DC2CALL Sqrt_Refined(u32 x);                                       /* 0x48F590 */
u32  DC2CALL Camera_Distance(gptr a, gptr b);                           /* 0x48F5C0 */
void DC2CALL Angles_LookAt(gptr target, gptr eye, gptr out);            /* 0x48F620 */
u32  DC2CALL Angle_XZ(gptr a, gptr b);                                  /* 0x48F6C0 */
u32  DC2CALL Angle_Seg(gptr q);                                         /* 0x48F710 */
void DC2CALL Obj_Set118(gptr o, u32 a, u32 b, u32 c);                   /* 0x48F880 */
void DC2CALL Obj_SetFeet(gptr o, u32 a, u32 b);                         /* 0x48FCA0 */
u32  DC2CALL Obj_AnchorFoot(gptr o);                                    /* 0x48FD20 */
void DC2CALL Generic_Handler(gptr o);                                   /* 0x490120 */
void DC2CALL Generic_Init(gptr o);                                      /* 0x4901B0 */
void DC2CALL Generic_Step(gptr o);                                      /* 0x4A56D0 */
void DC2CALL Generic_StepCb(gptr o);                                    /* 0x490270 */

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

/* script_ops.c, script_aot.c -- remaining built-ins */
void DC2CALL Obj_Init(u32 slot);                                        /* 0x48D820 */
gptr DC2CALL Obj_Part(gptr obj, u32 n);                                 /* 0x48D3F0 */
gptr DC2CALL Script_SelObj(gptr thr);                                   /* 0x48BE50 */
gptr DC2CALL Room_SoundSlot(u32 area, u32 room);                        /* 0x48CC30 */
void DC2CALL DrawSync(u32 mode);                                        /* 0x474B10 */
u32  DC2CALL ScrOp_Message(gptr thr);                                   /* 0x487650  11 */
u32  DC2CALL ScrOp_ObjSpawn(gptr thr);                                  /* 0x487A10  1A */
u32  DC2CALL ScrOp_ObjCall(gptr thr);                                   /* 0x48AC60  1B */
u32  DC2CALL ScrOp_WaitEvent(gptr thr);                                 /* 0x488F70  20 */
u32  DC2CALL ScrOp_AnimSet(gptr thr);                                   /* 0x488FA0  21 */
u32  DC2CALL ScrOp_Set87FC47(gptr thr);                                 /* 0x489000  22 */
u32  DC2CALL ScrOp_LoadModel(gptr thr);                                 /* 0x489020  23 */
u32  DC2CALL ScrOp_WaitB4A(gptr thr);                                   /* 0x489070  24 */
u32  DC2CALL ScrOp_Call46FBC0(gptr thr);                                /* 0x4890B0  25 */
u32  DC2CALL ScrOp_Timer(gptr thr);                                     /* 0x4890E0  26 */
u32  DC2CALL ScrOp_Fade(gptr thr);                                      /* 0x489110  27 */
u32  DC2CALL ScrOp_WaitFlag(gptr thr);                                  /* 0x489140  28 */
u32  DC2CALL ScrOp_Character(gptr thr);                                 /* 0x489180  29 */
u32  DC2CALL ScrOp_ObjMove(gptr thr);                                   /* 0x489410  2B */
u32  DC2CALL ScrOp_Menu(gptr thr);                                      /* 0x489460  2C */
u32  DC2CALL ScrOp_RoomReset(gptr thr);                                 /* 0x4894C0  2D */
u32  DC2CALL ScrOp_Call48C640(gptr thr);                                /* 0x48AD30  2E */
u32  DC2CALL ScrOp_Effect2F(gptr thr);                                  /* 0x489540  2F */
u32  DC2CALL ScrOp_AotDoor(gptr thr);                                   /* 0x4895E0  30 */
u32  DC2CALL ScrOp_Aot31(gptr thr);                                     /* 0x489710  31 */
u32  DC2CALL ScrOp_Aot32(gptr thr);                                     /* 0x4897E0  32 */
u32  DC2CALL ScrOp_Aot33(gptr thr);                                     /* 0x4898B0  33 */
u32  DC2CALL ScrOp_AotItem(gptr thr);                                   /* 0x4899A0  35 */
u32  DC2CALL ScrOp_Aot36(gptr thr);                                     /* 0x489B80  36 */
u32  DC2CALL ScrOp_Aot37(gptr thr);                                     /* 0x489C50  37 */
u32  DC2CALL ScrOp_Aot38(gptr thr);                                     /* 0x489D10  38 */
u32  DC2CALL ScrOp_Aot39(gptr thr);                                     /* 0x489DD0  39 */
u32  DC2CALL ScrOp_Aot3A(gptr thr);                                     /* 0x489EB0  3A */
u32  DC2CALL ScrOp_Aot3B(gptr thr);                                     /* 0x48A000  3B */
u32  DC2CALL ScrOp_AotRun(gptr thr);                                    /* 0x48A0D0  3E */
u32  DC2CALL ScrOp_CamMove(gptr thr);                                   /* 0x48A120  40 */
u32  DC2CALL ScrOp_WaitCutscene(gptr thr);                              /* 0x48AD60  41 */
u32  DC2CALL ScrOp_Cutscene(gptr thr);                                  /* 0x48ADC0  42 */
u32  DC2CALL ScrOp_Sound(gptr thr);                                     /* 0x48AE50  43 */
u32  DC2CALL ScrOp_ObjAnim(gptr thr);                                   /* 0x48A530  46 */
u32  DC2CALL ScrOp_LinkPart(gptr thr);                                  /* 0x48A580  47 */
u32  DC2CALL ScrOp_Random(gptr thr);                                    /* 0x48A640  48 */
u32  DC2CALL ScrOp_SoundSlot(gptr thr);                                 /* 0x48AF80  49 */
u32  DC2CALL ScrOp_WaitCamera(gptr thr);                                /* 0x48A720  4B */
u32  DC2CALL ScrOp_Call478B80(gptr thr);                                /* 0x48A770  4C */
u32  DC2CALL ScrOp_Call481D20(gptr thr);                                /* 0x48A7A0  4D */
u32  DC2CALL ScrOp_WaitFile(gptr thr);                                  /* 0x48A7D0  4E */
u32  DC2CALL ScrOp_EnemySet(gptr thr);                                  /* 0x48A7F0  4F */
u32  DC2CALL ScrOp_Op50(gptr thr);                                      /* 0x48A8C0  50 */
u32  DC2CALL ScrOp_Op51(gptr thr);                                      /* 0x48A930  51 */
u32  DC2CALL ScrOp_Op52(gptr thr);                                      /* 0x48A990  52 */
u32  DC2CALL ScrOp_Call48C600(gptr thr);                                /* 0x48AD00  53 */
u32  DC2CALL ScrOp_ObjsHold(gptr thr);                                  /* 0x48AA20  54 */
u32  DC2CALL ScrOp_Call48CE20(gptr thr);                                /* 0x48AE10  55 */
u32  DC2CALL ScrOp_Op56(gptr thr);                                      /* 0x48AAC0  56 */
u32  DC2CALL ScrOp_Call4529C0(gptr thr);                                /* 0x48AAE0  57 */
u32  DC2CALL ScrOp_PartHide(gptr thr);                                  /* 0x48AB10  58 */
u32  DC2CALL ScrOp_Texture(gptr thr);                                   /* 0x48AB90  59 */

/* anim.c -- keyframe animation */
void DC2CALL Anim_Set(gptr o, gptr anim, u32 frame, u32 fl, u32 blend);  /* 0x48DDE0 */
void DC2CALL Anim_Events(gptr o, s32 m);                                /* 0x48DE80 */
void DC2CALL Anim_Start(gptr o, gptr anim, u32 frame, u32 fl, u32 blend); /* 0x48DEF0 */
u32  DC2CALL Anim_Step(gptr o);                                         /* 0x48E100 */
u32  DC2CALL Anim_ApplyFrame(gptr o, u32 step);                         /* 0x48E3D0 */
gptr DC2CALL Matrix_ToAngles(gptr m, gptr out);                         /* 0x48F1D0 */
void DC2CALL Anim_BlendLinear(gptr a, gptr b, gptr out, s32 w);         /* 0x48FEA0 */
void DC2CALL Anim_BlendMatrix(gptr a, gptr b, gptr out, s32 w);         /* 0x48FF90 */
u32  DC2CALL Anim_FrameFlag(gptr o);                                    /* 0x48E970 */
void DC2CALL Anim_SaveLayer(u32 l);                                     /* 0x48E9C0 */
void DC2CALL Anim_LoadLayer(u32 l);                                     /* 0x48EBC0 */
void DC2CALL Anim_StartLayers(gptr anim0, gptr anim1, u32 frame, u32 fl, u32 blend, s32 w); /* 0x48EDC0 */
u32  DC2CALL Anim_StepLayers(s32 w);                                    /* 0x48EF50 */
void DC2CALL Player_AnimReset(void);                                    /* 0x48E930 */

/* file.c -- file loading */
u8   DC2CALL Rand8(void);                                               /* 0x48F750 */
u8   DC2CALL File_RequestLoad(u32 id);                                  /* 0x403E50 */
u8   DC2CALL File_RequestRaw(u32 id, gptr dest);                        /* 0x403E90 */
void DC2CALL File_Tick(void);                                           /* 0x403E40 */
void DC2CALL File_Process(void);                                        /* 0x404290 */
void DC2CALL File_Copy(gptr rec, gptr src);                             /* 0x4044C0 */
void DC2CALL File_Image(gptr rec, gptr src);                            /* 0x4044F0 */
void DC2CALL File_Clut(gptr rec, gptr src);                             /* 0x404680 */
void DC2CALL File_SoundBank(gptr rec, gptr src);                        /* 0x4046C0 */
void DC2CALL File_Stream(gptr rec, gptr src);                           /* 0x404770 */
void DC2CALL File_Unpack(gptr rec, gptr src);                           /* 0x4047F0 */
void DC2CALL File_Texture(gptr rec, gptr src);                          /* 0x404810 */
void DC2CALL File_PcBuffer(gptr rec, gptr src);                         /* 0x4048F0 */

/* room.c -- models, stages, rooms */
u32  DC2CALL File_IsBusy(void);                                         /* 0x403A20 */
void DC2CALL Enemy_LoadModel(u32 type, u32 variant);                    /* 0x482070 */
void DC2CALL Stage_Load(u32 w, u32 s);                                  /* 0x4822D0 */
void DC2CALL Room_Reset(void);                                          /* 0x481EC0 */
void DC2CALL Room_AllocObjects(gptr hdr);                               /* 0x481F10 */
void DC2CALL Room_SetPointers(gptr hdr);                                /* 0x481E10 */
void DC2CALL CamZones_Init(void);                                       /* 0x4034D0 */
void DC2CALL Aot_Reset(void);                                           /* 0x48AFD0 */
void DC2CALL Script_Skip(void);                                         /* 0x48BCB0 */
void DC2CALL Script_Event(void);                                        /* 0x48BD60 */
void DC2CALL Script_Tick(void);                                         /* 0x48BBF0 */
void DC2CALL Script_Init(void);                                         /* 0x48BB60 */

/* root.c -- root task, boot screens, save scan */
void DC2CALL Screen_LoadImage(u32 n);                                   /* 0x4828D0 */
void DC2CALL Root_Boot(gptr r);                                         /* 0x46D740 */
void DC2CALL Root_Logo(gptr r);                                         /* 0x46D7C0 */
void DC2CALL Root_Warning(gptr r);                                      /* 0x46D880 */
void DC2CALL Root_ToTitle(gptr r);                                      /* 0x46D970 */
void DC2CALL RootTask(void);                                            /* 0x46D710 */
void DC2CALL Save_ScanTask(void);                                       /* 0x403920 */

/* title.c -- title screen */
void DC2CALL Title_SetAnim(gptr t, u32 i, gptr a);                      /* 0x4A6370 */
u32  DC2CALL Title_AnimStep(gptr t, u32 i);                             /* 0x4A63A0 */
void DC2CALL Title_Draw(s32 x, s32 y, u32 color, gptr f);               /* 0x4A6210 */
void DC2CALL Title_NewGame(gptr t);                                     /* 0x4A5EC0 */
void DC2CALL Title_Load(gptr t);                                        /* 0x4A60F0 */
void DC2CALL Title_Option(gptr t);                                      /* 0x4A6180 */
void DC2CALL Title_Menu(gptr t);                                        /* 0x4A57B0 */
void DC2CALL Title_Movie(gptr t);                                       /* 0x4A5B50 */
void DC2CALL Title_Demo(gptr t);                                        /* 0x4A5BF0 */
void DC2CALL Title_Attract(gptr t);                                     /* 0x4A5B30 */
void DC2CALL Title_Init(gptr t);                                        /* 0x4A5760 */
void DC2CALL Title_Run(gptr t);                                         /* 0x4A5790 */
void DC2CALL TitleTask(void);                                           /* 0x4A5730 */

/* game.c -- game start, opening, game task */
void DC2CALL Game_Start(u32 mode);                                      /* 0x4826F0 */
void DC2CALL Pause_LoadScreen(void);                                    /* 0x4827F0 */
void DC2CALL Room_LoadDat(void);                                        /* 0x481C10 */
u32  DC2CALL Snd_RoomMusic(void);                                       /* 0x48CBD0 */
void DC2CALL Room_LoadExtra(u32 v);                                     /* 0x481CA0 */
void DC2CALL Door_Load(u32 n);                                          /* 0x481D20 */
void DC2CALL Armor_Load(u32 n);                                         /* 0x481DC0 */
void DC2CALL Checkpoint_Save(void);                                     /* 0x48F2C0 */
void DC2CALL Checkpoint_Load(void);                                     /* 0x48F330 */
void DC2CALL Area0_SetTables(void);                                     /* 0x48D170 */
void DC2CALL Area1_SetTables(void);                                     /* 0x48D1B0 */
void DC2CALL Area2_SetTables(void);                                     /* 0x48D1F0 */
void DC2CALL Area3_SetTables(void);                                     /* 0x48D230 */
void DC2CALL Area4_SetTables(void);                                     /* 0x48D270 */
void DC2CALL Area5_SetTables(void);                                     /* 0x48D2B0 */
void DC2CALL Area6_SetTables(void);                                     /* 0x48D2F0 */
void DC2CALL Area7_SetTables(void);                                     /* 0x48D330 */
void DC2CALL Area8_SetTables(void);                                     /* 0x48D370 */
void DC2CALL Area9_SetTables(void);                                     /* 0x48D3B0 */
void DC2CALL Snd_InitRoomTables(void);                                  /* 0x48BF00 */
void DC2CALL Game_ClearState(void);                                     /* 0x4510D0 */
void DC2CALL Game_NewState(void);                                       /* 0x450E00 */
void DC2CALL Opening_Init(gptr r);                                      /* 0x475660 */
void DC2CALL Opening_Movie(gptr r);                                     /* 0x4756A0 */
void DC2CALL Opening_Run(gptr r);                                       /* 0x475680 */
void DC2CALL Opening_ToGame(gptr r);                                    /* 0x475700 */
void DC2CALL OpeningTask(void);                                         /* 0x475630 */
void DC2CALL Game_Init(void);                                           /* 0x451120 */
void DC2CALL Game_AreaLoad(void);                                       /* 0x4512F0 */
void DC2CALL Game_RoomEnter(void);                                      /* 0x451330 */
void DC2CALL Game_Play(void);                                           /* 0x451790 */
void DC2CALL GameTask(void);                                            /* 0x4510F0 */

/* handler used for an opcode that is not reimplemented yet: in the hook
 * build it calls the original code, elsewhere it reports and stops the thread */
typedef u32 (DC2CALL *ScrOpFn)(gptr thr);
extern ScrOpFn g_scr_ops[96];
u32 DC2CALL ScrOp_Original(gptr thr);
#endif
