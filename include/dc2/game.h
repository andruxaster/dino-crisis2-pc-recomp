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
u32  DC2CALL Obj_CollideObjs(gptr o);                                   /* 0x452BD0 */
u32  DC2CALL Coll_Cylinders(gptr a, gptr b, gptr out, u32 heights);     /* 0x452EE0 */
u32  DC2CALL Seg_Intersect(gptr a, gptr b);                             /* 0x453290 */
u32  DC2CALL Obj_CollideWalls(gptr o);                                  /* 0x453430 */
u32  DC2CALL Seg_Side(gptr p, gptr b, gptr c);                          /* 0x453EE0 */
u32  DC2CALL Circle_Seg(gptr s, gptr p);                                /* 0x453F50 */
u32  DC2CALL Seg_Cross(gptr a, gptr b, gptr out);                       /* 0x4540B0 */
u32  DC2CALL Wall_Push(gptr s, gptr p, gptr v, gptr out);               /* 0x4541F0 */
u32  DC2CALL Poly_RayCount(gptr poly, gptr r);                          /* 0x454870 */
u32  DC2CALL Floor_Height(gptr a, gptr b, u32 mask, gptr kind);         /* 0x454420 */
gptr DC2CALL K2_Alloc(void);                                            /* 0x48DA40 */
gptr DC2CALL K2_Reuse(void);                                            /* 0x48D480 */
gptr DC2CALL K2_AllocBack(void);                                        /* 0x48D580 */
gptr DC2CALL Effect_Attach(gptr o, gptr d);                             /* 0x48DA00 */
gptr DC2CALL Effect_Drops(gptr o, u32 a, u32 b);                        /* 0x48DC80 */
gptr DC2CALL Part_Alloc(void);                                          /* 0x48D740 */
void DC2CALL Model_Setup(gptr o, gptr d);                               /* 0x48D8B0 */
void DC2CALL K2_Unlink(gptr k);                                         /* 0x48D620 */
void DC2CALL Lights_Off(void);                                          /* 0x41C9B0 */
void DC2CALL Eff_KillKind(u32 kind);                                    /* 0x448580 */

/* player.c -- the player object */
void DC2CALL Player_Setup(gptr o);                                      /* 0x47FE10 */
void DC2CALL Player_Breath(gptr o);                                     /* 0x47A7E0 */
void DC2CALL Player_Handler(gptr o);                                    /* 0x47CE40 */
void DC2CALL Obj_SaveFeet(gptr o);                                      /* 0x48FCE0 */
void DC2CALL Player_Weapon(gptr o);                                     /* 0x47AF40 */
void DC2CALL Eff_FreeAll(void);                                         /* 0x47AF60 */
void DC2CALL Eff_Free(gptr e);                                          /* 0x48D730 */
gptr DC2CALL Eff_Alloc(void);                                           /* 0x47C150 */
u32  DC2CALL Cone_HitX(u32 a, u32 b, gptr p, gptr q);                   /* 0x47C1A0 */
u32  DC2CALL Cone_HitZ(u32 a, u32 b, gptr p, gptr q);                   /* 0x47C250 */
s32  DC2CALL Target_Compare(gptr a, gptr b);                            /* 0x47BF30 */
void DC2CALL Target_Find(gptr o, u32 range);                            /* 0x47BDE0 */
void DC2CALL Arm_Relax(gptr o, u32 arm);                                /* 0x47C4C0 */
void DC2CALL Arm_Aim(gptr o, u32 arm, u32 yaw);                         /* 0x47C2C0 */
void DC2CALL Player_Init(gptr o);                                       /* 0x47D2F0 */
void DC2CALL Player_Normal(gptr o);                                     /* 0x47D500 */
void DC2CALL Player_State2(gptr o);                                     /* 0x47DA60 */
void DC2CALL Player_State5(gptr o);                                     /* 0x47DBD0 */
void DC2CALL Player_State6(gptr o);                                     /* 0x47DC50 */
u32  DC2CALL Damage_Scale(u32 d);                                       /* 0x479700 */
u32  DC2CALL Pad_Shake(void);                                           /* 0x479D70 */
void DC2CALL Player_HitLight(gptr o);                                   /* 0x47D6A0 */
void DC2CALL Player_HitDown(gptr o);                                    /* 0x47D820 */
void DC2CALL Player_Die(gptr o);                                        /* 0x47DAB0 */
void DC2CALL Player_Scripted(gptr o);                                   /* 0x4796C0 */
void DC2CALL Obj_Stop(gptr o);                                          /* 0x4796E0 */
u32  DC2CALL Item_Find(gptr list, u32 count, u32 id);                   /* 0x495CB0 */
u32  DC2CALL Ammo_Have(u32 n);                                          /* 0x496240 */
u32  DC2CALL Player_SubDone(void);                                      /* 0x47C860 */
u32  DC2CALL Wall_Ahead(gptr o, u32 dist);                              /* 0x479DA0 */
u32  DC2CALL Step_Ahead(gptr o, u32 len, u32 w, u32 yaw);              /* 0x479FC0 */
void DC2CALL Player_Stand(gptr o);                                      /* 0x47DC70 */
void DC2CALL Player_Walk(gptr o);                                       /* 0x47E290 */
void DC2CALL Player_TurnBack(gptr o);                                   /* 0x47EED0 */
void DC2CALL Obj_ShadowOff(gptr o);                                     /* 0x48F8B0 */
void DC2CALL Player_Climb(gptr o);                                      /* 0x47EF90 */
void DC2CALL Player_Push(gptr o);                                       /* 0x47FC70 */
void DC2CALL Player_Turn(gptr o);                                       /* 0x47E890 */
void DC2CALL Player_Back(gptr o);                                       /* 0x47EBB0 */
void DC2CALL Player_LoadWeapons(gptr o);                                /* 0x47ADA0 */

/* weapon.c -- weapon and sub-weapon set-up */
gptr DC2CALL Light_Reset(u32 i);                                        /* 0x41C970 */
void DC2CALL Weapon_SetNone(gptr o);                                    /* 0x4B2C20 */
void DC2CALL Weapon_SetGun(gptr o);                                     /* 0x4AC980 */
void DC2CALL Weapon_SetLight(gptr o);                                   /* 0x4AE2F0 */
void DC2CALL Weapon_SetTwin(gptr o);                                    /* 0x4AEF10 */
void DC2CALL Weapon_SetHeavy(gptr o);                                   /* 0x4B1C90 */
void DC2CALL Sub_SetKnives(gptr o);                                     /* 0x4B2C30 */
void DC2CALL Sub_SetKnife(gptr o);                                      /* 0x4B2FC0 */
void DC2CALL Sub_SetGrenade(gptr o);                                    /* 0x4B33C0 */
void DC2CALL Sub_SetMine(gptr o);                                       /* 0x4B3A10 */
void DC2CALL Sub_SetKnives2(gptr o);                                    /* 0x4B3D00 */

/* crt.c -- C runtime functions */
void DC2CALL Crt_qsort(gptr base, u32 num, u32 width, gptr cmp);        /* 0x4DDBD4 */

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

/* zone.c */
u32  DC2CALL Wall_Cross(gptr s, gptr w);                               /* 0x487160 */
u32  DC2CALL Line_Blocked(gptr o, gptr a, gptr b, u32 low);            /* 0x486FB0 */
u32  DC2CALL Zone_Find(gptr o);                                        /* 0x486D00 */
void DC2CALL Zone_Paths(gptr o, gptr tab);                             /* 0x486EC0 */
void DC2CALL Zone_Track(gptr o, gptr t);                               /* 0x4869F0 */
u32  DC2CALL Zone_TrackPoint(gptr o, gptr p);                          /* 0x486C00 */
u32  DC2CALL Zone_Update(gptr o);                                      /* 0x486870 */
/* player.c: aiming */
u32  DC2CALL Angle_Step(u32 cur, u32 step, u32 target);                /* 0x47CAE0 */
void DC2CALL Player_WeaponState(gptr o);                               /* 0x47ADE0 */
void DC2CALL Player_SubState(gptr o);                                  /* 0x47AF20 */
void DC2CALL Player_Aim(gptr o);                                       /* 0x47AE00 */

/* object.c: room change, props */
void DC2CALL Eff_KillRoom(u32 unused);                                 /* 0x448620 */
void DC2CALL Prop_Handler(gptr o);                                     /* 0x446170 */
void DC2CALL Prop_Init(gptr o);                                        /* 0x4461F0 */
void DC2CALL Prop_Run(gptr o);                                         /* 0x4462B0 */
void DC2CALL Prop_Play(gptr o);                                        /* 0x446250 */
void DC2CALL Prop_PlayTimed(gptr o);                                   /* 0x446270 */
/* game.c */
void DC2CALL Room_SoundInit(void);                                     /* 0x48CA10 */

/* hits */
u32  DC2CALL Seg_Hit(gptr s, gptr w);                                  /* 0x47C6C0 */
u32  DC2CALL Wall_Between(gptr a, gptr b);                             /* 0x47C550 */
void DC2CALL Stat_Shot(void);                                          /* 0x496400 */
void DC2CALL Hit_Sort(gptr b, u32 lo, u32 hi);                         /* 0x47C030 */
u32  DC2CALL Weapon_Hit(gptr m, gptr pos, u32 kind);                   /* 0x47B430 */

u32  DC2CALL Eff_WallHit(gptr e);                                     /* 0x453060 */
gptr DC2CALL Effect_Spawn(gptr pos, gptr data, u32 a, u32 b, u32 c, u32 d); /* 0x48DBC0 */
void DC2CALL Noise_Make(u32 x, u32 y, u32 z);                          /* 0x48B830 */
void DC2CALL Thrown_Update(gptr o);                                    /* 0x47AFA0 */

/* gun.c */
u32  DC2CALL Ammo_Check(u32 n);                                        /* 0x4961D0 */
void DC2CALL Ammo_Use(u32 n);                                          /* 0x4962B0 */
gptr DC2CALL Flash_Alloc(void);                                        /* 0x478AA0 */
gptr DC2CALL Flash_AllocObj(gptr o);                                   /* 0x478AF0 */
gptr DC2CALL Flash_Start(gptr o, u32 k);                               /* 0x478A60 */
gptr DC2CALL Flash_StartFor(gptr o, u32 k, u32 n);                     /* 0x478B40 */
void DC2CALL WState_0(gptr o);                                         /* 0x4AA7A0 */
void DC2CALL WState_1(gptr o);                                         /* 0x4AB450 */
void DC2CALL WState_2(gptr o);                                         /* 0x4ABD10 */
void DC2CALL WState_3(gptr o);                                         /* 0x4AC9A0 */
void DC2CALL WState_4(gptr o);                                         /* 0x4AD6A0 */
void DC2CALL WState_5(gptr o);                                         /* 0x4AE320 */
void DC2CALL WState_6(gptr o);                                         /* 0x4AEF60 */
void DC2CALL WState_7(gptr o);                                         /* 0x4AF500 */
void DC2CALL WState_8(gptr o);                                         /* 0x4B0490 */
void DC2CALL WState_9(gptr o);                                         /* 0x4B08D0 */
void DC2CALL WState_10(gptr o);                                        /* 0x4B0ED0 */
void DC2CALL WState_11(gptr o);                                        /* 0x4B1CB0 */
void DC2CALL Handgun_Fire(gptr o);                                     /* 0x4AB490 */
void DC2CALL Gun_Ready(gptr o);                                        /* 0x4ACCC0 */
void DC2CALL Gun_Hold(gptr o);                                         /* 0x4AD0D0 */
void DC2CALL Gun_Lower(gptr o);                                        /* 0x4AABD0 */
void DC2CALL Gun_NextTarget(gptr o);                                   /* 0x4AE9E0 */
void DC2CALL Gun_TurnBack(gptr o);                                     /* 0x4B27C0 */
void DC2CALL Gun_Walk(gptr o);                                         /* 0x4AB640 */
void DC2CALL Gun_Back(gptr o);                                         /* 0x4AB870 */
void DC2CALL Gun_Turn(gptr o);                                         /* 0x4ABAA0 */
void DC2CALL Shotgun_Fire(gptr o);                                     /* 0x4ABD50 */
void DC2CALL Shotgun_Ready(gptr o);                                    /* 0x4AC140 */
void DC2CALL Shotgun_Lower(gptr o);                                    /* 0x4AC550 */
void DC2CALL Shotgun_Walk(gptr o);                                     /* 0x4ABF00 */
void DC2CALL Shotgun_Back(gptr o);                                     /* 0x4AC750 */
void DC2CALL MGun_Fire(gptr o);                                        /* 0x4AE360 */
void DC2CALL MGun_Ready(gptr o);                                       /* 0x4ADCE0 */
void DC2CALL MGun_Lower(gptr o);                                       /* 0x4AE0F0 */
void DC2CALL MGun_Walk(gptr o);                                        /* 0x4AE5A0 */
void DC2CALL MGun_Back(gptr o);                                        /* 0x4AE7D0 */
void DC2CALL Twin_Fire(gptr o);                                        /* 0x4AEFA0 */
void DC2CALL Twin_Ready(gptr o);                                       /* 0x4AF7B0 */
void DC2CALL Twin_Lower(gptr o);                                       /* 0x4AFDF0 */
void DC2CALL Twin_Walk(gptr o);                                        /* 0x4AF260 */
void DC2CALL Twin_Back(gptr o);                                        /* 0x4AEC70 */
void DC2CALL Weapon7_Fire(gptr o);                                     /* 0x4AF540 */
void DC2CALL Weapon7_Walk(gptr o);                                     /* 0x4AFBC0 */
void DC2CALL Weapon7_Back(gptr o);                                     /* 0x4AFFF0 */
void DC2CALL Heavy_Hold(gptr o);                                       /* 0x4B23A0 */
void DC2CALL Heavy_Turn(gptr o);                                       /* 0x4B1A50 */
void DC2CALL Heavy10_Fire(gptr o);                                     /* 0x4B0F10 */
void DC2CALL Heavy10_Ready(gptr o);                                    /* 0x4B12C0 */
void DC2CALL Heavy10_Lower(gptr o);                                    /* 0x4B1690 */
void DC2CALL Heavy10_Walk(gptr o);                                     /* 0x4B10C0 */
void DC2CALL Heavy10_Back(gptr o);                                     /* 0x4B1850 */
void DC2CALL Heavy11_Fire(gptr o);                                     /* 0x4B1CF0 */
void DC2CALL Heavy11_Ready(gptr o);                                    /* 0x4B1FD0 */
void DC2CALL Heavy11_Lower(gptr o);                                    /* 0x4B2600 */
void DC2CALL Heavy11_Walk(gptr o);                                     /* 0x4B2890 */
void DC2CALL Heavy11_Back(gptr o);                                     /* 0x4B2A60 */
void DC2CALL Bare_Ready(gptr o);                                       /* 0x4AA7C0 */
void DC2CALL Bare_Walk(gptr o);                                        /* 0x4AADD0 */
void DC2CALL Bare_Back(gptr o);                                        /* 0x4AAFD0 */
void DC2CALL Bare_Turn(gptr o);                                        /* 0x4AB1E0 */

void DC2CALL Spray_Walk(gptr o);                       /* 0x4AD9B0 */
void DC2CALL Spray_Back(gptr o);                       /* 0x4AD370 */
void DC2CALL Rifle_Turn(gptr o);                       /* 0x4B0220 */
void DC2CALL Rifle_Walk(gptr o);                       /* 0x4B0AE0 */
void DC2CALL Rifle_Back(gptr o);                       /* 0x4B0CE0 */
void DC2CALL Weapon3_Fire(gptr o);                     /* 0x4AC9E0 */
void DC2CALL Weapon4_Fire(gptr o);                     /* 0x4AD6E0 */
void DC2CALL Weapon8_Fire(gptr o);                     /* 0x4B04D0 */
void DC2CALL Weapon9_Fire(gptr o);                     /* 0x4B0910 */

#endif
