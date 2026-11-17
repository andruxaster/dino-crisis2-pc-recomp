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
void DC2CALL Game_SetMode723CC0(u32 v);                                /* 0x46FC50 */
void DC2CALL Game_Leave(void);                                         /* 0x471EC0 */
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
void DC2CALL Sub_AmmoUse(u32 n);                                       /* 0x496350 */
gptr DC2CALL Effect_SpawnAt(gptr pos, gptr data, u32 unused);          /* 0x48DB80 */
gptr DC2CALL Light_Alloc(void);                                        /* 0x48D410 */
void DC2CALL Sub1_Use(gptr o);                                         /* 0x4B2CF0 */
void DC2CALL Sub2_Use(gptr o);                                         /* 0x4B3030 */
void DC2CALL Sub7_Use(gptr o);                                         /* 0x4B3DD0 */
void DC2CALL Sub8_Use(gptr o);                                         /* 0x4B41F0 */
void DC2CALL Sub5_Use(gptr o);                                         /* 0x4B38E0 */
void DC2CALL Sub9_Use(gptr o);                                         /* 0x4B4600 */
void DC2CALL Sub1_PutBack(gptr o);                                     /* 0x4B40A0 */
void DC2CALL Sub2_PutBack(gptr o);                                     /* 0x4B3330 */
void DC2CALL Sub3_PutBack(gptr o);                                     /* 0x4B3650 */
void DC2CALL Sub4_PutBack(gptr o);                                     /* 0x4B3CA0 */
void DC2CALL Sub8_PutBack(gptr o);                                     /* 0x4B44F0 */
void DC2CALL Sub9_PutBack(gptr o);                                     /* 0x4B4580 */
void DC2CALL Sub3_Use(gptr o);                                         /* 0x4B3440 */
void DC2CALL Sub4_Use(gptr o);                                         /* 0x4B36B0 */
void DC2CALL Sub6_Use(gptr o);                                         /* 0x4B3AA0 */
u32  DC2CALL Mines_Place(gptr o, gptr pos);                            /* 0x47C8F0 */
void DC2CALL Player_Crouch(gptr o);                                    /* 0x47F0F0 */
void DC2CALL Player_WaterStand(gptr o);                                /* 0x47E050 */
void DC2CALL Player_WaterWalk(gptr o);                                 /* 0x47E640 */

/* door transitions (door.c) */
gptr DC2CALL Door_Dest(void);                                          /* 0x48B820 */
gptr DC2CALL Room_Name(u32 area, u32 room);                            /* 0x495C90 */
void DC2CALL Lights_ListReset(void);                                   /* 0x46B890 */
void DC2CALL Camera_Aim(gptr eye, gptr target, gptr ang, gptr dist);   /* 0x4038E0 */
void DC2CALL Door_SetupPlayer(gptr d, u32 idx, gptr pos);              /* 0x41E6B0 */
void DC2CALL Door_SetupModel(gptr d, u32 idx, gptr model, u32 x, u32 yaw);/* 0x41E790 */
void DC2CALL Door_SetCamera(gptr cam);                                 /* 0x41E820 */
void DC2CALL Door_SetLight(gptr d);                                    /* 0x41E8F0 */
void DC2CALL Door_Init(gptr d);                                        /* 0x41E310 */
void DC2CALL Door_Run(gptr d);                                         /* 0x41E9C0 */
void DC2CALL Door_AreaSwitch(gptr d);                                  /* 0x41EAD0 */
void DC2CALL Room_LoadTask(gptr d);                                    /* 0x41EBB0 */
void DC2CALL Door_Music(gptr d);                                       /* 0x41EC50 */
void DC2CALL Door_End(gptr d);                                         /* 0x41ED10 */
void DC2CALL AOT_Check(void);                                          /* 0x48B040 */
void DC2CALL Aot_Tick(void);                                           /* 0x48AFF0 */
void DC2CALL AOT_Door(gptr e);                                         /* 0x48B390 */
void DC2CALL AOT_Event(gptr e);                                        /* 0x48B460 */
void DC2CALL AOT_Message(gptr e);                                      /* 0x48B480 */
void DC2CALL AOT_Flag(gptr e);                                         /* 0x48B4C0 */
void DC2CALL AOT_Item(gptr e);                                         /* 0x48B4E0 */
void DC2CALL AOT_EffectStart(gptr e);                                  /* 0x48B530 */
void DC2CALL AOT_Kind7(gptr e);                                        /* 0x48B590 */
void DC2CALL AOT_Menu(gptr e);                                         /* 0x48B5B0 */
void DC2CALL AOT_Wait(gptr e);                                         /* 0x48B5D0 */
void DC2CALL AOT_Kind10(gptr e);                                       /* 0x48B620 */
void DC2CALL AOT_Kind11(gptr e);                                       /* 0x48B7D0 */
gptr DC2CALL Eff_AllocBack(void);                                      /* 0x48D6C0 */
void DC2CALL Obj_RunAll(void);                                         /* 0x471EF0 */
void DC2CALL Prop_RunAll(void);                                        /* 0x471F90 */
void DC2CALL Eff_RunAll(void);                                         /* 0x471FE0 */
void DC2CALL ObjEff_RunAll(void);                                      /* 0x472040 */
void DC2CALL Game_Frame(void);                                         /* 0x472FD0 */
void DC2CALL Snd_Tick(void);                                           /* 0x48C700 */
s32  DC2CALL Voice_Volume(u32 kind);                                   /* 0x48D000 */
void DC2CALL Voice_Play(u32 kind, u32 id);                             /* 0x48D070 */
void DC2CALL Voice_Stop(void);                                         /* 0x48D130 */
gptr DC2CALL CamZones_FindForCamera(gptr c, gptr zones);              /* 0x4035B0 */
void DC2CALL CamZones_Check(gptr c, gptr zones);                      /* 0x4035F0 */
void DC2CALL Camera_Tick(void);                                        /* 0x403570 */
void DC2CALL Camera_Load(void);                                        /* 0x403AC0 */
void DC2CALL Continue_SetAnim(gptr a, gptr f);                        /* 0x4522C0 */
u32  DC2CALL Continue_AnimStep(gptr a);                                /* 0x4522E0 */
void DC2CALL Continue_Draw(s32 x, s32 y, u32 color, gptr f);           /* 0x452170 */
u32  DC2CALL Text_Length(gptr s);                                      /* 0x471A90 */
void DC2CALL Game_Continue(void);                                      /* 0x451990 */
gptr DC2CALL Obj_AllocEnemy(void);                                     /* 0x48D7B0 */
void DC2CALL EnemySet_Cries(gptr s);                                   /* 0x472130 */
void DC2CALL EnemySet_Cry(u32 kind, u32 obj);                          /* 0x4720B0 */
void DC2CALL EnemySet_Light(gptr p);                                   /* 0x4721B0 */
void DC2CALL EnemySet_Way(gptr a, gptr b, gptr out);                   /* 0x472BB0 */
u32  DC2CALL EnemySet_FindGroup(gptr grp, s32 n);                      /* 0x472C60 */
void DC2CALL EnemySet_Tick(void);                                      /* 0x472230 */
void DC2CALL EnemySet_Small(void);                                     /* 0x472D20 */
gptr DC2CALL Flash_Play(gptr owner, gptr data);                        /* 0x478B80 */
gptr DC2CALL Prop_Attach(gptr obj, u32 arg);                           /* 0x48DAB0 */
gptr DC2CALL Prop_SpawnC(u32 kind, gptr pos, u32 a, u32 b, u32 c, u32 d);/* 0x48DC20 */
u32  DC2CALL Obj_InSight(gptr o, gptr target, u32 dist, u32 ang);      /* 0x47A780 */
void DC2CALL Score_Event(gptr o);                                      /* 0x404980 */
void DC2CALL Score_Kill(gptr o);                                       /* 0x404AF0 */
u32  DC2CALL Enemy_HitDir(gptr o);                                     /* 0x479D00 */
u32  DC2CALL Obj_HitDir(gptr o);                                       /* 0x479CB0 */
u32  DC2CALL Player_Bleed(u32 n);                                      /* 0x4797D0 */
void DC2CALL Obj_DeathFade(gptr o);                                    /* 0x48FBB0 */
u32  DC2CALL Enemy_FindState(gptr o, u32 sub);                         /* 0x47AD00 */
void DC2CALL Light_Flash(gptr o);                                      /* 0x414D20 */
void DC2CALL Layers_Assign(void);                                      /* 0x46B960 */
u32  DC2CALL Enemy_AttackHit(gptr o);                                  /* 0x479830 */
u32  DC2CALL Sphere_Hit(gptr a, gptr b, u32 h, u32 r);                 /* 0x452FF0 */
void DC2CALL GameOver_Start(u32 fade);                                 /* 0x4529C0 */
void DC2CALL Raptor_Handler(gptr o);                                   /* 0x41F430 */
void DC2CALL Raptor_Init(gptr o);                                      /* 0x41F660 */
void DC2CALL Raptor_Normal(gptr o);                                    /* 0x41F960 */
void DC2CALL Raptor_Hit(gptr o);                                       /* 0x41F9A0 */
void DC2CALL Raptor_Die(gptr o);                                       /* 0x41F9D0 */
void DC2CALL Raptor_Think(gptr o);                                     /* 0x41FB20 */
void DC2CALL Raptor_Grab(gptr o);                                      /* 0x420A00 */
void DC2CALL Raptor_Pounce(gptr o);                                    /* 0x420A40 */
void DC2CALL Raptor_LungeHit(gptr o, gptr pl);                         /* 0x421290 */
void DC2CALL Raptor_SideBite(gptr o);                                  /* 0x421460 */
void DC2CALL Raptor_SideBiteHit(gptr o, gptr pl);                      /* 0x4215A0 */
void DC2CALL Raptor_Leap(gptr o);                                      /* 0x4216B0 */
void DC2CALL Raptor_LeapHit(gptr o, gptr pl);                          /* 0x421870 */
void DC2CALL Raptor_Tail(gptr o);                                      /* 0x421C10 */
void DC2CALL Raptor_TailHit(gptr o, gptr pl);                          /* 0x421CC0 */
void DC2CALL Raptor_Climb(gptr o);                                     /* 0x421F50 */
void DC2CALL Raptor_JumpOver(gptr o);                                  /* 0x4221E0 */
void DC2CALL Raptor_Hurt(gptr o);                                      /* 0x422400 */
void DC2CALL Raptor_HurtStand(gptr o);                                 /* 0x422430 */
void DC2CALL Raptor_HurtDown(gptr o);                                  /* 0x4226E0 */
void DC2CALL Raptor_Knock(gptr o);                                     /* 0x4228C0 */
void DC2CALL Raptor_KnockStand(gptr o);                                /* 0x4228F0 */
void DC2CALL Raptor_KnockDown(gptr o);                                 /* 0x422BF0 */
void DC2CALL Raptor_Blow(gptr o);                                      /* 0x422E00 */
void DC2CALL Raptor_BlowStand(gptr o);                                 /* 0x422E30 */
void DC2CALL Raptor_HurtAir(gptr o);                                   /* 0x423080 */
void DC2CALL Raptor_Burn(gptr o);                                      /* 0x423300 */
void DC2CALL Raptor_Drown(gptr o);                                     /* 0x4234B0 */
void DC2CALL Raptor_Wait(gptr o);                                      /* 0x41FF30 */
void DC2CALL Raptor_Walk(gptr o);                                      /* 0x420190 */
void DC2CALL Raptor_Attack(gptr o);                                    /* 0x4203B0 */
void DC2CALL Raptor_Jump(gptr o);                                      /* 0x420410 */
void DC2CALL Raptor_Cry(gptr o);                                       /* 0x4204A0 */
void DC2CALL Raptor_Back(gptr o);                                      /* 0x4205D0 */
void DC2CALL Raptor_Eat(gptr o);                                       /* 0x420660 */
void DC2CALL Raptor_Enter(gptr o);                                     /* 0x4207E0 */
void DC2CALL Raptor_Feint(gptr o);                                     /* 0x420960 */
/* the oviraptor (src/game/oviraptor.c) */
void DC2CALL Ovi_Handler(gptr o);                           /* 0x441FB0 */
void DC2CALL Ovi_Init(gptr o);                              /* 0x442080 */
void DC2CALL Ovi_Normal(gptr o);                            /* 0x4423D0 */
void DC2CALL Ovi_Hit(gptr o);                               /* 0x442410 */
void DC2CALL Ovi_Die(gptr o);                               /* 0x442450 */
void DC2CALL Ovi_Think(gptr o);                             /* 0x4425D0 */
void DC2CALL Ovi_Grab(gptr o);                              /* 0x4427E0 */
void DC2CALL Ovi_Walk(gptr o);                              /* 0x442840 */
void DC2CALL Ovi_Attack(gptr o);                            /* 0x442B20 */
void DC2CALL Ovi_Climb(gptr o);                             /* 0x442BF0 */
void DC2CALL Ovi_JumpOver(gptr o);                          /* 0x442E90 */
void DC2CALL Ovi_Jump(gptr o);                              /* 0x442B60 */
void DC2CALL Ovi_Enter(gptr o);                             /* 0x4429B0 */
void DC2CALL Ovi_Look(gptr o);                              /* 0x4430A0 */
void DC2CALL Ovi_Hide(gptr o);                              /* 0x4432D0 */
void DC2CALL Ovi_Turn(gptr o);                              /* 0x4433D0 */
void DC2CALL Ovi_LeapBack(gptr o, gptr p);                  /* 0x4434B0 */
void DC2CALL Ovi_Peck(gptr o, gptr p);                      /* 0x4436B0 */
void DC2CALL Ovi_GrabBite(gptr o, gptr p);                  /* 0x4435D0 */
void DC2CALL Ovi_Pounce(gptr o, gptr p);                    /* 0x443730 */
void DC2CALL Ovi_PounceCatch(gptr o, gptr p);               /* 0x443C70 */
void DC2CALL Ovi_Lunge(gptr o, gptr p);                     /* 0x443E30 */
void DC2CALL Ovi_LungeHit(gptr o, gptr p);                  /* 0x443EF0 */
void DC2CALL Ovi_Kick(gptr o, gptr p);                      /* 0x443FA0 */
void DC2CALL Ovi_KickHit(gptr o, gptr p);                   /* 0x4440D0 */
void DC2CALL Ovi_BackJump(gptr o, gptr p);                  /* 0x4441A0 */
void DC2CALL Ovi_BackCatch(gptr o, gptr p);                 /* 0x444330 */
void DC2CALL Ovi_HurtStand(gptr o);                         /* 0x4444A0 */
void DC2CALL Ovi_HurtDown(gptr o);                          /* 0x4447F0 */
void DC2CALL Ovi_Hurt(gptr o);                              /* 0x444470 */
void DC2CALL Ovi_KnockStand(gptr o);                        /* 0x4449B0 */
void DC2CALL Ovi_KnockDown(gptr o);                         /* 0x444B80 */
void DC2CALL Ovi_Knock(gptr o);                             /* 0x444980 */
void DC2CALL Ovi_Blow(gptr o);                              /* 0x444D00 */
void DC2CALL Ovi_BlowStand(gptr o);                         /* 0x444D30 */
void DC2CALL Ovi_BlowDown(gptr o);                          /* 0x444F90 */
void DC2CALL Ovi_HurtAir(gptr o);                           /* 0x445190 */
void DC2CALL Obj_TurnToPoint(gptr o, u32 max, gptr pos);              /* 0x48F440 */
void DC2CALL Door_Task(void);                                          /* 0x41E2D0 */
void DC2CALL Door_Common(gptr d);                                      /* 0x41CB70 */
void DC2CALL Door_SwingAMove(gptr d);                                  /* 0x41CA70 */
void DC2CALL Door_SwingA(gptr d);                                      /* 0x41CA50 */
void DC2CALL Door_SwingBMove(gptr d);                                  /* 0x41CC60 */
void DC2CALL Door_SwingB(gptr d);                                      /* 0x41CC40 */
void DC2CALL Door_Shutter(gptr d, s32 step);                           /* 0x41D090 */
void DC2CALL Door_ShutterSlow(gptr d);                                 /* 0x41D080 */
void DC2CALL Door_ShutterFast(gptr d);                                 /* 0x41D210 */
void DC2CALL Door_Slide(gptr d, u32 dir);                              /* 0x41D230 */
void DC2CALL Door_SlideL(gptr d);                                      /* 0x41D220 */
void DC2CALL Door_SlideR(gptr d);                                      /* 0x41D420 */
void DC2CALL Door_Common2(gptr d);                                     /* 0x41D460 */
void DC2CALL Door_Double(gptr d, s32 step, s32 lim, s32 lim2);         /* 0x41D4F0 */
void DC2CALL Door_DoubleA(gptr d);                                     /* 0x41D430 */
void DC2CALL Door_DoubleB(gptr d);                                     /* 0x41D670 */
void DC2CALL Door_DoubleC(gptr d);                                     /* 0x41D6A0 */
void DC2CALL Door_Double2(gptr d, s32 step);                           /* 0x41D6F0 */
void DC2CALL Door_Double2A(gptr d);                                    /* 0x41D6D0 */
void DC2CALL Door_Double2B(gptr d);                                    /* 0x41D820 */
void DC2CALL Door_Double2C(gptr d);                                    /* 0x41D840 */
void DC2CALL Door_CamTarget(u32 x, u32 y, u32 z);           /* 0x41CEA0 */
void DC2CALL Door_CamEye(u32 x, u32 y, u32 z);              /* 0x41D990 */
void DC2CALL Door_LiftDown(gptr d);                         /* 0x41CD60 */
void DC2CALL Door_LiftUp(gptr d);                           /* 0x41CF20 */
void DC2CALL Door_Drop(gptr d);                             /* 0x41D860 */
void DC2CALL Door_WaitHidden(gptr d);                       /* 0x41DA10 */
void DC2CALL Door_Wait(gptr d);                             /* 0x41DB90 */
void DC2CALL Door_Message(gptr d);                          /* 0x41DAF0 */
void DC2CALL Door_Walk(gptr d);                             /* 0x41DC70 */
void DC2CALL Door_StairsDown(gptr d);                       /* 0x41DDF0 */
void DC2CALL Door_StairsUp(gptr d);                         /* 0x41E050 */
void DC2CALL Door_ScoreBox(gptr d);                         /* 0x41F040 */
void DC2CALL Door_Score(gptr d);                            /* 0x41ED70 */
u32  DC2CALL Crouch_Fire(gptr o);                                      /* 0x47FA80 */

void DC2CALL Spray_Walk(gptr o);                       /* 0x4AD9B0 */
void DC2CALL Spray_Back(gptr o);                       /* 0x4AD370 */
void DC2CALL Rifle_Turn(gptr o);                       /* 0x4B0220 */
void DC2CALL Rifle_Walk(gptr o);                       /* 0x4B0AE0 */
void DC2CALL Rifle_Back(gptr o);                       /* 0x4B0CE0 */
void DC2CALL Weapon3_Fire(gptr o);                     /* 0x4AC9E0 */
void DC2CALL Weapon4_Fire(gptr o);                     /* 0x4AD6E0 */
void DC2CALL Weapon8_Fire(gptr o);                     /* 0x4B04D0 */
void DC2CALL Weapon9_Fire(gptr o);                     /* 0x4B0910 */


/* object effects (effect.c) */
void DC2CALL Nothing(gptr e);                                          /* 0x402520 */
void DC2CALL ObjEff_Turning(gptr e);                                   /* 0x474BB0 */
void DC2CALL ObjEff_Turn(gptr e);                                      /* 0x474BD0 */
void DC2CALL ObjEff_Blink(gptr e);                                     /* 0x474BE0 */
void DC2CALL ObjEff_BlinkMove(gptr e);                                 /* 0x474C00 */
void DC2CALL ObjEff_BlinkDark(gptr e);                                 /* 0x474C80 */
void DC2CALL ObjEff_BlinkLit(gptr e);                                  /* 0x474CA0 */
void DC2CALL ObjEff_Shade(gptr e, u32 v);                              /* 0x474CC0 */
void DC2CALL ObjEff_ThrownInit(gptr e);                                /* 0x474D50 */
void DC2CALL ObjEff_ThrownMove(gptr e);                                /* 0x474DC0 */
void DC2CALL ObjEff_Thrown(gptr e);                                    /* 0x474E50 */
void DC2CALL ObjEff_Debris(gptr e);                                    /* 0x474E70 */
void DC2CALL ObjEff_DebrisInit(gptr e);                                /* 0x474EA0 */
void DC2CALL ObjEff_DebrisMove(gptr e);                                /* 0x474F10 */
void DC2CALL ObjEff_InHand(gptr e);                                    /* 0x474FB0 */
void DC2CALL ObjEff_InHandInit(gptr e);                                /* 0x474FD0 */
void DC2CALL ObjEff_InHandHold(gptr e);                                /* 0x475080 */


/* effect slots (k2.c) */
void DC2CALL K2_ToWorld(gptr k, gptr out);                             /* 0x448100 */
gptr DC2CALL K2_OwnerObj(gptr k);                                      /* 0x448140 */
void DC2CALL K2_SyncParent(gptr k);                                    /* 0x4481A0 */
void DC2CALL K2_PushFront(gptr k);                                     /* 0x4481E0 */
void DC2CALL K2_Light(gptr k, u32 c);                                  /* 0x448240 */
void DC2CALL K2Step_SetAngles(gptr k);                                 /* 0x4482B0 */
void DC2CALL K2Step_SetYaw(gptr k);                                    /* 0x4482E0 */
void DC2CALL K2Step_Accel(gptr k);                                     /* 0x448300 */
void DC2CALL K2Step_SpinAccel(gptr k);                                 /* 0x448330 */
void DC2CALL K2Step_Rise(gptr k);                                      /* 0x448350 */
void DC2CALL K2Step_Grow(gptr k);                                      /* 0x4483A0 */
void DC2CALL K2Step_GrowLaterInit(gptr k);                             /* 0x448430 */
void DC2CALL K2Step_GrowLater(gptr k);                                 /* 0x448470 */
void DC2CALL K2Step_Hold(gptr k);                                      /* 0x448520 */
void DC2CALL K2_MatrixAngles(gptr m, gptr out);                        /* 0x448760 */
s32 DC2CALL K2_AnimStep(gptr k);                                       /* 0x48DD10 */
void DC2CALL K2_SetAnim(gptr k, gptr list);                            /* 0x48DCE0 */
u32 DC2CALL Pos_Hidden(gptr pos);                                      /* 0x48F8C0 */
void DC2CALL K2_UpdateHidden(gptr k);                                  /* 0x48F9B0 */
void DC2CALL K2_Handler(gptr k);                                       /* 0x448900 */
void DC2CALL K2_Init(gptr k);                                          /* 0x4489B0 */
void DC2CALL K2_Move(gptr k);                                          /* 0x448B70 */
void DC2CALL K2_FadeOut(gptr k);                                       /* 0x448BE0 */
void DC2CALL K2_Handler2(gptr k);                                      /* 0x448C70 */
void DC2CALL K2_SplashInit(gptr k);                                    /* 0x448CC0 */
void DC2CALL K2_SplashFall(gptr k);                                    /* 0x448E10 */
void DC2CALL K2_FloatA(gptr k);                                        /* 0x448EB0 */
void DC2CALL K2_FloatB(gptr k);                                        /* 0x448FC0 */
void DC2CALL K2_Splash(gptr k);                                        /* 0x4490D0 */
void DC2CALL K2_Emitter(gptr k);                                       /* 0x449120 */
void DC2CALL K2_EmitterStart(gptr k);                                  /* 0x449140 */
void DC2CALL K2_EmitterStep(gptr k);                                   /* 0x449180 */
void DC2CALL K2_Emitter2(gptr k);                                      /* 0x4494C0 */
void DC2CALL K2_Emitter2Start(gptr k);                                 /* 0x449530 */
void DC2CALL K2_StepNext(gptr k);                                      /* 0x449650 */
void DC2CALL K2_Emitter2Step(gptr k);                                  /* 0x449660 */
void DC2CALL K2Drops_Start(gptr k);                                     /* 0x44B4C0 */
void DC2CALL K2Drops_Back(gptr k);                                      /* 0x44B540 */
void DC2CALL K2Drops_Spawn(gptr pos, gptr col);                         /* 0x44B660 */
void DC2CALL K2Drops_Init(gptr k);                                      /* 0x44B5A0 */
void DC2CALL K2Drops_Drop(gptr k, gptr pos, gptr col, gptr ang);        /* 0x44B790 */
void DC2CALL K2Drops_Run(gptr k);                                       /* 0x44B730 */
void DC2CALL K2Bubbles_Start(gptr k);                                   /* 0x44B9E0 */
void DC2CALL K2Bubbles_Keep(gptr k);                                    /* 0x44BA90 */
void DC2CALL K2Bubble_Init(gptr k);                                     /* 0x44BB60 */
void DC2CALL K2Bubble_Rise(gptr k);                                     /* 0x44BCE0 */
void DC2CALL K2Bubble_Sink(gptr k);                                     /* 0x44BD70 */
void DC2CALL K2Bubble_Wait(gptr k);                                     /* 0x44BE00 */
void DC2CALL K2Bubble_Shrink(gptr k);                                   /* 0x44BE10 */
void DC2CALL K2_Drops(gptr k);                                          /* 0x44B4A0 */
void DC2CALL K2Stuck_Place(gptr k);                                     /* 0x44BEF0 */
void DC2CALL K2Stuck_Init(gptr k);                                      /* 0x44BE30 */
void DC2CALL K2Stuck_Run(gptr k);                                       /* 0x44BF30 */
void DC2CALL K2_Stuck(gptr k);                                          /* 0x44BF80 */
void DC2CALL EffSlots_Step(gptr e);                                     /* 0x40EF20 */
void DC2CALL EffSlots_Start(gptr e);                                    /* 0x40EEF0 */
void DC2CALL EffSlots_Wait(gptr e);                                     /* 0x40F210 */
void DC2CALL EffSlots(gptr e);                                          /* 0x40EED0 */
void DC2CALL EffArea_Start(gptr e);                                     /* 0x4140E0 */
void DC2CALL EffArea_Run(gptr e);                                       /* 0x4141A0 */
void DC2CALL K2Puff_Init(gptr k);                                       /* 0x449FD0 */
void DC2CALL K2Puff_Move(gptr k);                                       /* 0x44A160 */
void DC2CALL K2Puff_Wait(gptr k);                                       /* 0x44A210 */
void DC2CALL K2Puff_Shrink(gptr k);                                     /* 0x44A250 */
void DC2CALL K2_Puff(gptr k);                                           /* 0x449F90 */
void DC2CALL K2Glow_Init(gptr k);                                       /* 0x44A2A0 */
void DC2CALL K2Glow_Dim(gptr k);                                        /* 0x44A300 */
void DC2CALL K2Glow_Brighten(gptr k);                                   /* 0x44A330 */
void DC2CALL K2Glow_Place(gptr k);                                      /* 0x44A360 */
void DC2CALL K2_Glow(gptr k);                                           /* 0x44A280 */
void DC2CALL K2Lone_Init(gptr k);                                       /* 0x44A400 */
void DC2CALL K2Lone_Move(gptr k);                                       /* 0x44C0A0 */
void DC2CALL K2Lone_End(gptr k);                                        /* 0x44CAF0 */
void DC2CALL K2_Lone(gptr k);                                           /* 0x44A380 */
void DC2CALL K2Spark_New(gptr pos, gptr c0, gptr c1, gptr c2, gptr w);  /* 0x44A9E0 */
void DC2CALL K2Spark_Draw(gptr k, gptr pos, gptr c0, gptr c1, gptr c2, gptr w);/* 0x44AAD0 */
void DC2CALL K2Sparks_Init(gptr k);                                     /* 0x44A4A0 */
void DC2CALL K2Sparks_Turn(gptr k);                                     /* 0x44A620 */
void DC2CALL K2Sparks_Draw(gptr k);                                     /* 0x44A890 */
void DC2CALL K2Sparks_Off(gptr k);                                      /* 0x44AFC0 */
void DC2CALL K2Burst_New(gptr vel, gptr col, gptr t);                   /* 0x44AFD0 */
void DC2CALL K2Bursts_Init(gptr k);                                     /* 0x44AD30 */
void DC2CALL K2Burst_Draw(gptr k, gptr pos, gptr vel, gptr col, gptr tm);/* 0x44B030 */
void DC2CALL K2Bursts_Run(gptr k);                                      /* 0x44AE80 */
void DC2CALL K2_Sparks(gptr k);                                         /* 0x44A480 */
void DC2CALL K2_Detach(gptr k);                                         /* 0x448080 */
void DC2CALL Eff_Off(gptr e);                                           /* 0x48D470 */
/* misc.c */
void DC2CALL Camera_Reset(void);                                        /* 0x403480 */
void DC2CALL Camera_ZoneTick(void);                                     /* 0x403520 */
void DC2CALL Masks_Setup(gptr data, u32 n, gptr pixels);                /* 0x4036F0 */
void DC2CALL Cutscene_End(void);                                        /* 0x403A30 */
void DC2CALL Gauge_Clear(void);                                         /* 0x404940 */
void DC2CALL Gauge_Show(void);                                          /* 0x406020 */
void DC2CALL Gauge_Reset(void);                                         /* 0x407170 */
void DC2CALL Room_SetLighting(void);                                    /* 0x46B8C0 */
void DC2CALL Shake_Set(u32 a, u32 b, u32 c);                            /* 0x46FBC0 */
void DC2CALL Shake_Tick(void);                                          /* 0x452370 */
void DC2CALL Shake_Off(void);                                           /* 0x452BA0 */
void DC2CALL Dim_Tick(void);                                            /* 0x4509C0 */
void DC2CALL Fade_Set(u32 a, u32 b, u32 c);                             /* 0x450A70 */
void DC2CALL Fade_Draw(gptr s);                                         /* 0x450B10 */
void DC2CALL Fade_Tick(void);                                           /* 0x450AF0 */
void DC2CALL Eff0_Trail(void);                                          /* 0x445D10 */
void DC2CALL Lights_Unmark(void);                                       /* 0x478790 */
void DC2CALL Snd_ReleaseAll(void);                                      /* 0x48C080 */
void DC2CALL Snd_SetFade(u32 slot, u32 a, u32 b, u32 c);                /* 0x48C5D0 */
void DC2CALL Strm_SetFade(u32 slot, u32 a, u32 b, u32 c);               /* 0x48CE20 */
void DC2CALL Strm_Start(u32 n, u32 unused);                             /* 0x48CD70 */
void DC2CALL Strm_StartMask(u32 mask);                                  /* 0x48C9C0 */
void DC2CALL Snd_LeaveRoom(void);                                       /* 0x48CA40 */
void DC2CALL Snd_FadeAll(void);                                         /* 0x48CB40 */
void DC2CALL Snd_FreeBank(u32 n);                                       /* 0x48CEB0 */
void DC2CALL Snd_FreeStream(u32 n);                                     /* 0x48CF10 */
void DC2CALL Obj_UpdateHidden(gptr o);                                  /* 0x48F910 */
void DC2CALL Eff_UpdateHidden(gptr e);                                  /* 0x48F980 */
void DC2CALL Hidden_UpdateAll(void);                                    /* 0x48FA70 */
void DC2CALL Light_SetFar(u32 c);                                       /* 0x48FB50 */
u32 DC2CALL Area_SoundArea(u32 area, u32 room);                         /* 0x491B10 */
u32 DC2CALL Text_Left(gptr t);                                          /* 0x494DE0 */
void DC2CALL Msg_Clear(void);                                           /* 0x471A30 */
void DC2CALL Msg_ClearBox(void);                                        /* 0x471A50 */
void DC2CALL Msg_SetScale(u32 x, u32 y);                                /* 0x471A70 */
void DC2CALL Msg_Reset(void);                                           /* 0x470CB0 */
void DC2CALL Msg_Tick(void);                                            /* 0x470CF0 */
gptr DC2CALL Message_Show(u32 x, u32 y, gptr text, u32 kind);           /* 0x4718F0 */
void DC2CALL Msg_Flush(void);                                           /* 0x471AC0 */
void DC2CALL Movie_Start(u32 n);                                        /* 0x473130 */
void DC2CALL Movie_Tick(void);                                          /* 0x4731F0 */
void DC2CALL MsgSlot_Step(gptr s);                                      /* 0x470D30 */
void DC2CALL Box_Type(void);                                            /* 0x470D50 */
void DC2CALL Box_Wait(void);                                            /* 0x470F40 */
void DC2CALL Box_Prompt(void);                                          /* 0x470F70 */
void DC2CALL Box_Release(void);                                         /* 0x4710D0 */
void DC2CALL Box_YesNo(void);                                           /* 0x4710F0 */
void DC2CALL Box_Choose(void);                                          /* 0x4712B0 */
void DC2CALL Box_Close(void);                                           /* 0x4713C0 */
void DC2CALL Se_Info(u32 id, gptr r);                                   /* 0x48C220 */
void DC2CALL Se_Calc(gptr o, u32 vol, gptr pan, gptr pvol, u32 range);  /* 0x48C2D0 */
u32 DC2CALL Se_Remap(gptr o, u32 id);                                   /* 0x48C3B0 */
void DC2CALL Se_Start(u32 h, gptr o, gptr e, u32 pan, u32 vol, u32 id); /* 0x48C4F0 */
void DC2CALL Se_Play(gptr o, u32 id);                                   /* 0x48C0B0 */
void DC2CALL Se_PlayFlat(u32 id, u32 vol, u32 pan);                     /* 0x48C640 */
void DC2CALL Se_StopGroup(u32 g);                                       /* 0x48C600 */
void DC2CALL Strm_Halt(u32 n);                                          /* 0x48CE60 */
u32 DC2CALL Text_Draw(gptr s, u32 x, u32 y, u32 n, u32 col, u32 z, u32 mode);/* 0x494BA0 */
void DC2CALL Lights_Run(void);                                          /* 0x478CF0 */
void DC2CALL Pad_Map(gptr src, gptr dst);                               /* 0x478DF0 */
void DC2CALL Input_Frame(void);                                         /* 0x478BF0 */
u32 DC2CALL Light_Try(gptr p, gptr l, u32 slot);                        /* 0x46BC20 */
void DC2CALL Part_Lights(gptr p, u32 keep);                             /* 0x46BAC0 */
void DC2CALL Obj_PickLights(gptr unused);                               /* 0x46BA00 */
u32 DC2CALL Color_Pulse(u32 a, u32 b, u32 sh);                          /* 0x48FAE0 */
void DC2CALL Hud_DrawArea(u32 x, u32 y, u32 w, u32 h);                  /* 0x406810 */
void DC2CALL Hud_Tile(gptr r, u32 col, u32 semi);                       /* 0x406C90 */
void DC2CALL Hud_Sprite(gptr r, u32 u, u32 v, u32 clut, u32 col, u32 semi);/* 0x406E30 */
void DC2CALL Hud_Picture(gptr list, u32 x, u32 y, u32 tp, u32 clut, u32 col);/* 0x406D50 */
gptr DC2CALL Gauge_Bar(u32 x, u32 y);                                   /* 0x4062C0 */
void DC2CALL Gauge_Draw(u32 x, u32 y);                                  /* 0x406050 */
void DC2CALL Game_InitStock(void);                                      /* 0x496410 */
void DC2CALL Hud_Glyph(gptr r, u32 u, u32 v, u32 clut, u32 col, u32 semi); /* 0x406F10 */
void DC2CALL Hud_Line(u32 x0, u32 y0, u32 x1, u32 y1, u32 col, u32 semi); /* 0x4070A0 */
void DC2CALL Hud_Number(u32 x, u32 y, u32 n, u32 lead, u32 col, u32 v);  /* 0x406890 */
void DC2CALL Hud_SmallNum(u32 x, u32 y, u32 n, u32 lead, u32 col, u32 v);/* 0x406A10 */
void DC2CALL Hud_GlyphNum(u32 x, u32 y, u32 n, u32 lead, u32 col, u32 v);/* 0x406B50 */
void DC2CALL Hud_Weapon(void);                                          /* 0x404CC0 */
void DC2CALL Obj_ToView(gptr o, gptr m);                                /* 0x4A71B0 */
void DC2CALL World_Matrices(void);                                      /* 0x4A6DF0 */
void DC2CALL Mat_ToView(gptr src, gptr dst);                            /* 0x4460A0 */
void DC2CALL Z_Fixed(void);                                             /* 0x4A82F0 */
void DC2CALL Z_Min3(void);                                              /* 0x4A8310 */
void DC2CALL Z_Min4(void);                                              /* 0x4A8360 */
void DC2CALL Z_Max3(void);                                              /* 0x4A83C0 */
void DC2CALL Z_Max4(void);                                              /* 0x4A8410 */
void DC2CALL Mesh_TriTex(gptr e, u32 n, gptr zfn);                      /* 0x4A8470 */
void DC2CALL Mesh_QuadTex(gptr e, u32 n, gptr zfn);                     /* 0x4A8690 */
void DC2CALL Mesh_QuadFlat(gptr e, u32 n, gptr zfn);                    /* 0x4A8930 */
void DC2CALL Eff_DrawAll(void);                                         /* 0x4A7840 */
void DC2CALL Model_Tris(gptr e, u32 n, u32 tp);                         /* 0x4A8C00 */
void DC2CALL Model_Quads(gptr e, u32 n, u32 tp);                        /* 0x4A8F40 */
void DC2CALL Model_DrawGroups(gptr m, u32 mask, u32 tp);                /* 0x4A8BA0 */
void DC2CALL Obj_DrawAll(void);                                         /* 0x4A7510 */
void DC2CALL Obj_DrawShadows(void);                                     /* 0x4A7ED0 */
void DC2CALL Prop_DrawAll(void);                                        /* 0x4A7A20 */
void DC2CALL Draw_Handlers(void);                                       /* 0x4A7330 */
void DC2CALL Dim_Bars(void);                                            /* 0x4A7390 */
void DC2CALL Masks_Draw(gptr c);                                        /* 0x403760 */
void DC2CALL Rope_Draw(void);                                           /* 0x445E10 */
void DC2CALL Text_DrawAll(void);                                        /* 0x471440 */
void DC2CALL Frame_Draw(void);                                          /* 0x4A7250 */
void DC2CALL Timer_Set(u32 frames);                                     /* 0x405DE0 */
void DC2CALL Eff_Detach(gptr e);                                        /* 0x474B20 */
void DC2CALL Obj_MoveReset(gptr o, u32 n);                              /* 0x47A8C0 */
u32 DC2CALL Room_CountUp(void);                                         /* 0x48FDA0 */
void DC2CALL Item_Compact(gptr list, u32 n);                            /* 0x495D70 */
void DC2CALL Item_Merge(void);                                          /* 0x495DF0 */
void DC2CALL Panel_Sprite(u32 x, u32 y, u32 u, u32 v, u32 small, u32 col);/* 0x41F080 */
void DC2CALL Panel_Digits(u32 v, u32 x, u32 y, u32 n, u32 col);         /* 0x41F1C0 */
void DC2CALL Panel_Bonus(gptr d);                                       /* 0x41F170 */
void DC2CALL Panel_Kills(gptr d);                                       /* 0x41F390 */
void DC2CALL Panel_Points(gptr d);                                      /* 0x41F3E0 */
u32 DC2CALL Floor_Cross(gptr a, gptr b, gptr seg, u32 who);             /* 0x4542F0 */
void DC2CALL Timer_Draw(void);                                          /* 0x405E40 */
void DC2CALL Boss_Bar(u32 x, u32 y);                                    /* 0x4065A0 */
void DC2CALL Boss_Gauge(u32 x, u32 y);                                  /* 0x406520 */
void DC2CALL Menu_Open(u32 a, u32 b, u32 c, u32 d, u32 e);              /* 0x495E90 */
u32  DC2CALL Menu_Pad(gptr m);      /* 0x494630 */
void DC2CALL Menu_Tile(gptr r, u32 color, u32 semi, u32 z); /* 0x494730 */
void DC2CALL Menu_ShiftLine(u32 dx, u32 y, u32 h); /* 0x4956C0 */
u32  DC2CALL Item_CountUsed(gptr p, u32 n); /* 0x495CE0 */
void DC2CALL Menu_Flash(gptr m);    /* 0x491D10 */
void DC2CALL MenuMode_Init(gptr m); /* 0x491B30 */
void DC2CALL MenuMode_Enter(gptr m); /* 0x491C00 */
void DC2CALL MenuMode_Screen(gptr m); /* 0x491E20 */
void DC2CALL Menu_Task(void);       /* 0x491950 */
void DC2CALL Menu_Sprite(gptr r, u32 u, u32 v, u32 tp, u32 color, u32 semi, u32 z); /* 0x495040 */
void DC2CALL Menu_Sprite80(gptr r, u32 u, u32 v, u32 tp, u32 color, u32 semi, u32 z); /* 0x495130 */
void DC2CALL Menu_DrawFrames(gptr f, u32 x, u32 y, u32 tpage, u32 tp, u32 color, u32 z); /* 0x495220 */
void DC2CALL Menu_PolyG4(gptr v, u32 c0, u32 c1, u32 c2, u32 c3, u32 semi, u32 z); /* 0x494850 */
void DC2CALL Menu_Line(u32 x0, u32 y0, u32 x1, u32 y1, u32 color, u32 semi, u32 z); /* 0x4949E0 */
void DC2CALL Menu_PolyF3(gptr v, u32 color, u32 z); /* 0x495980 */
void DC2CALL Menu_PolyG3(gptr v, u32 c0, u32 c1, u32 c2, u32 z); /* 0x495AA0 */
void DC2CALL Menu_DrawArea(u32 x, u32 y, u32 w, u32 h, u32 z); /* 0x4946A0 */
void DC2CALL Msg_Concat(gptr d, gptr a, gptr b); /* 0x494E80 */
u32  DC2CALL Menu_SmallText(gptr s, u32 x, u32 y, u32 n, u32 color, u32 z); /* 0x494EE0 */
void DC2CALL Menu_Arrows(u32 x, u32 y); /* 0x495350 */
u32  DC2CALL Flag_CountRange(u32 from, u32 to); /* 0x495BE0 */
void DC2CALL Flag_ListRange(u32 from, u32 to, u32 max, gptr out); /* 0x495C30 */
u32  DC2CALL Item_FindFree(gptr p, u32 n); /* 0x495D10 */
u32  DC2CALL Item_FindRoom(gptr p, u32 n, u32 id); /* 0x495D30 */
u32  DC2CALL Menu_Number8(u32 x, u32 y, u32 digits, u32 lead, u32 color, u32 value, u32 z); /* 0x492E20 */
u32  DC2CALL Menu_Number11(u32 x, u32 y, u32 digits, u32 lead, u32 color, u32 value, u32 z); /* 0x492F80 */
u32  DC2CALL Menu_Number12(u32 x, u32 y, u32 digits, u32 lead, u32 color, u32 value, u32 z); /* 0x4930E0 */
void DC2CALL Menu_YesNo(u32 x, u32 y, u32 sel, u32 z); /* 0x494AC0 */
void DC2CALL Menu_Points(u32 x, u32 y, u32 color); /* 0x494150 */
void DC2CALL Menu_PauseList(gptr m); /* 0x493F40 */
void DC2CALL Menu_Sweep(u32 x, u32 y, u32 phase, u32 dir); /* 0x492850 */
void DC2CALL Menu_Header(gptr m);   /* 0x492520 */
void DC2CALL Menu_Gauges(gptr m, u32 x, u32 y); /* 0x492930 */
void DC2CALL Menu_Frame(gptr m);    /* 0x492A80 */
void DC2CALL Menu_Backdrop(gptr m); /* 0x492BC0 */
void DC2CALL Menu_Panels(void);     /* 0x493C10 */
void DC2CALL Menu_Weapons(gptr m, u32 x, u32 y, u32 z); /* 0x493240 */
void DC2CALL Menu_Health(u32 x, u32 y); /* 0x4942E0 */
void DC2CALL Menu_PauseScreen(gptr m); /* 0x493BC0 */
void DC2CALL Menu_Border(gptr r, u32 pos, u32 n, u32 s, u32 color); /* 0x4957A0 */
void DC2CALL Pause_Describe(u32 i, u32 x, u32 y, u32 w); /* 0x4920F0 */
void DC2CALL Screen_Pause(gptr m);  /* 0x491E50 */
void DC2CALL Screen_Leave(gptr m);  /* 0x492180 */
void DC2CALL MenuMode_Leave(gptr m); /* 0x492330 */
void DC2CALL Screen_Equip(gptr m);  /* 0x496930 */
void DC2CALL Equip_Init(gptr m);    /* 0x496970 */
void DC2CALL Equip_Choose(gptr m);  /* 0x496A30 */
void DC2CALL Equip_ArrowsR(u32 x, u32 y); /* 0x497ED0 */
void DC2CALL Equip_ArrowsL(u32 x, u32 y); /* 0x497F80 */
void DC2CALL Equip_RingItem(gptr m, u32 x, u32 y, u32 sel, u32 idx, u32 color, u32 z); /* 0x498030 */
void DC2CALL Equip_Ring(gptr m, u32 main); /* 0x497AE0 */
void DC2CALL Equip_Options(gptr m); /* 0x4985C0 */
void DC2CALL Equip_Stats(gptr m, u32 main); /* 0x4976C0 */
void DC2CALL Equip_WeaponRing(gptr m); /* 0x496D90 */
void DC2CALL Equip_SubRing(gptr m); /* 0x497210 */
void DC2CALL Equip_WeaponSel(gptr m); /* 0x496EB0 */
void DC2CALL Equip_SubSel(gptr m);  /* 0x497310 */
void DC2CALL Equip_WeaponView(gptr m); /* 0x497120 */
void DC2CALL Equip_SubView(gptr m); /* 0x497480 */
void DC2CALL Equip_Back(gptr m);    /* 0x497570 */
void DC2CALL Equip_NoWeapon(gptr m); /* 0x497620 */
void DC2CALL Screen_Items(gptr m);  /* 0x498710 */
void DC2CALL Items_Init(gptr m);    /* 0x498750 */
void DC2CALL Items_Choose(gptr m);  /* 0x498840 */
void DC2CALL Items_List(gptr m);    /* 0x498B40 */
void DC2CALL Items_Use(gptr m);     /* 0x498C80 */
void DC2CALL Items_Sort(gptr m);    /* 0x498E10 */
void DC2CALL Items_None(gptr m);    /* 0x499040 */
void DC2CALL Files_List(gptr m);    /* 0x499100 */
void DC2CALL Files_Sel(gptr m);     /* 0x499260 */
void DC2CALL Files_View(gptr m);    /* 0x499430 */
void DC2CALL Files_None(gptr m);    /* 0x499520 */
void DC2CALL Files_Cant(gptr m);    /* 0x4995C0 */
void DC2CALL Items_Back(gptr m);    /* 0x4996C0 */
void DC2CALL Items_Draw(gptr m);    /* 0x499770 */
void DC2CALL Items_Panel(gptr m);   /* 0x499D00 */
void DC2CALL Files_Frame(gptr m);   /* 0x499D60 */
void DC2CALL Files_Draw(gptr m);    /* 0x499F80 */
void DC2CALL Menu_Ring(u32 angle, u32 x, u32 y, u32 r1, u32 r2, u32 color); /* 0x495410 */
void DC2CALL Text_Ascii(u32 x, u32 y, u32 col, gptr s); /* 0x471B30 */
u32  DC2CALL Zone_FileSpot(u32 id); /* 0x48B280 */
void DC2CALL Screen_Memos(gptr m);  /* 0x49A540 */
void DC2CALL Memos_Init(gptr m);    /* 0x49A570 */
void DC2CALL Memos_List(gptr m);    /* 0x49A5C0 */
void DC2CALL Memos_Read(gptr m);    /* 0x49A740 */
u32  DC2CALL Text_Pages(gptr t);    /* 0x49A8D0 */
void DC2CALL Memos_None(gptr m);    /* 0x49A900 */
void DC2CALL Memos_Back(gptr m);    /* 0x49A9A0 */
void DC2CALL Memos_Draw(gptr m);    /* 0x49A9F0 */
void DC2CALL Memos_Frame(gptr m);   /* 0x49AFA0 */
u32  DC2CALL Memo_Type(gptr t, u32 x, u32 y, u32 page, u32 count, u32 color, u32 z, u32 arrows); /* 0x49B270 */
void DC2CALL Screen_Map(gptr m);    /* 0x49B620 */
u32  DC2CALL Map_CountAreas(void);  /* 0x49B770 */
u32  DC2CALL Map_NextArea(gptr m);  /* 0x49B7A0 */
u32  DC2CALL Map_PrevArea(gptr m);  /* 0x49B810 */
void DC2CALL Map_Init(gptr m);      /* 0x49B880 */
void DC2CALL Map_View(gptr m);      /* 0x49B940 */
void DC2CALL Map_Other(gptr m);     /* 0x49BB80 */
void DC2CALL Map_Shown(gptr m);     /* 0x49BBB0 */
void DC2CALL Map_ZoomLoad(gptr m);  /* 0x49BBC0 */
void DC2CALL Map_ZoomWait(gptr m);  /* 0x49BC20 */
void DC2CALL Map_Zoom(gptr m);      /* 0x49BC50 */
void DC2CALL Map_None(gptr m);      /* 0x49BF90 */
void DC2CALL Map_Back(gptr m);      /* 0x49C110 */
void DC2CALL Map_Locate(gptr m, u32 ry, u32 x, u32 z, u32 scale, u32 size, gptr out); /* 0x49CEB0 */
void DC2CALL Map_Doors(gptr m);     /* 0x49CD20 */
void DC2CALL Map_ZoomDraw(gptr m);  /* 0x49C150 */
void DC2CALL Text_AsciiZ(gptr s, u32 x, u32 y, u32 color, u32 z); /* 0x471C90 */
void DC2CALL Map_AreaDoors(gptr d, u32 area, u32 n); /* 0x49DD50 */
void DC2CALL Map_Rooms(u32 area, u32 n); /* 0x49E210 */
void DC2CALL Map_Legend(gptr m, u32 x, u32 y); /* 0x49D360 */
void DC2CALL Map_Key(gptr m, u32 x, u32 y); /* 0x49D480 */
void DC2CALL Map_Arrow(gptr m, u32 x, u32 y); /* 0x49DFB0 */
void DC2CALL Map_Frame(gptr m);     /* 0x49D5D0 */
void DC2CALL Map_Area0(gptr m);     /* 0x49D170 */
void DC2CALL Map_Area1(gptr m);     /* 0x49DE90 */
void DC2CALL Map_Area2(gptr m);     /* 0x49E360 */
void DC2CALL Map_Area3(gptr m);     /* 0x49E430 */
void DC2CALL Map_Area4(gptr m);     /* 0x49E550 */
void DC2CALL Map_Area4b(gptr m);    /* 0x49E790 */
void DC2CALL Map_Area5(gptr m);     /* 0x49E900 */
void DC2CALL Map_Area6(gptr m);     /* 0x49E9B0 */
void DC2CALL Map_Area7(gptr m);     /* 0x49EF50 */
void DC2CALL Map_Area8(gptr m);     /* 0x49F050 */
void DC2CALL Map_Area9(gptr m);     /* 0x49F1C0 */
void DC2CALL MenuMode_Pickup(gptr m); /* 0x49F270 */
void DC2CALL Pickup_Clear(u32 v);   /* 0x49F9F0 */
void DC2CALL Game_ModeStock(void);                                      /* 0x452450 */

u32  DC2CALL Angle_DiffTo(gptr a, gptr b, u32 ang);                  /* 0x48F3E0 */
u32  DC2CALL Angle_Diff(u32 a, u32 b);                               /* 0x48F410 */
u32  DC2CALL Value_Approach(gptr p, u32 step, u32 target);           /* 0x48F4B0 */
void DC2CALL Vec_RotY(gptr out, gptr in, u32 ang);                   /* 0x48F470 */
u32  DC2CALL Wall_Dist(gptr o, u32 ang, u32 mask);                   /* 0x454B50 */
void DC2CALL Score_Hit(gptr o, u32 n);                               /* 0x404A20 */
void DC2CALL Boss_Flash(gptr o, u32 alt);                            /* 0x415A40 */
void DC2CALL Enemy_BiteOn(gptr o);                                   /* 0x42B710 */
void DC2CALL Enemy_BiteOff(gptr o);                                  /* 0x433960 */
u32 DC2CALL Vec_StepXZ(gptr a, gptr b, s32 speed, gptr out);         /* 0x47A630 */
void DC2CALL Combo_Kill(u32 type);                                     /* 0x412CC0 */
void DC2CALL Enemy_Remove(gptr o);                                   /* 0x47AD60 */
void DC2CALL Obj_ClearHits(gptr o);                                  /* 0x478840 */
void DC2CALL Player_Release(gptr o);                                 /* 0x48E780 */
void DC2CALL Player_Hurt(gptr o, u32 dmg, u32 how, u32 sub);         /* 0x48E7E0 */
void DC2CALL Player_Hold(gptr o, u32 dmg, u32 cb);                   /* 0x48E8A0 */
/* allosaurus.c */
void DC2CALL Allo_Handler(gptr o);                              /* 0x4397C0 */
void DC2CALL Allo_Init(gptr o);                                 /* 0x439A80 */
void DC2CALL Allo_Normal(gptr o);                               /* 0x439CA0 */
void DC2CALL Allo_Hit(gptr o);                                  /* 0x439CE0 */
void DC2CALL Allo_Die(gptr o);                                  /* 0x439D00 */
void DC2CALL Allo_Grab(gptr o);                                 /* 0x439D20 */
void DC2CALL Allo_ThinkIdle(gptr o);                            /* 0x439D50 */
void DC2CALL Allo_ThinkWalk(gptr o);                            /* 0x43A040 */
void DC2CALL Allo_ThinkRun(gptr o);                             /* 0x43A240 */
u32  DC2CALL Allo_Steer(gptr o);                                /* 0x43A4C0 */
void DC2CALL Allo_ThinkBack(gptr o);                            /* 0x43ABA0 */
void DC2CALL Allo_ThinkLunge(gptr o);                           /* 0x43B390 */
void DC2CALL Allo_Idle(gptr o);                                 /* 0x439F70 */
void DC2CALL Allo_Walk(gptr o);                                 /* 0x43A1B0 */
void DC2CALL Allo_Run(gptr o);                                  /* 0x43A390 */
void DC2CALL Allo_Leap(gptr o);                                 /* 0x43A7E0 */
void DC2CALL Allo_Turn(gptr o);                                 /* 0x43AA10 */
void DC2CALL Allo_Back(gptr o);                                 /* 0x43AC70 */
void DC2CALL Allo_Roar(gptr o);                                 /* 0x43AD40 */
void DC2CALL Allo_Enter(gptr o);                                /* 0x43AE00 */
void DC2CALL Allo_Anger(gptr o);                                /* 0x43AE60 */
void DC2CALL Allo_Bite(gptr o);                                 /* 0x43AF00 */
void DC2CALL Allo_Thrown(gptr pl);                              /* 0x43B2E0 */
void DC2CALL Allo_Lunge(gptr o);                                /* 0x43B3C0 */
void DC2CALL Allo_SnapUp(gptr o);                               /* 0x43B4E0 */
void DC2CALL Allo_Snap(gptr o);                                 /* 0x43B5A0 */
void DC2CALL Allo_Toss(gptr o);                                 /* 0x43B640 */
void DC2CALL Allo_Sidestep(gptr o);                             /* 0x43B910 */
void DC2CALL Allo_Dash(gptr o);                                 /* 0x43B9B0 */
void DC2CALL Allo_BiteHit(gptr o);                              /* 0x43BA30 */
void DC2CALL Allo_LungeHit(gptr o);                             /* 0x43BBC0 */
void DC2CALL Allo_SnapUpHit(gptr o);                            /* 0x43BC40 */
void DC2CALL Allo_SnapHit(gptr o);                              /* 0x43BCC0 */
void DC2CALL Allo_SideHit(gptr o);                              /* 0x43BD30 */
void DC2CALL Allo_LeapHit(gptr o);                              /* 0x43BDB0 */
void DC2CALL Allo_Damage(gptr o);                               /* 0x43BE00 */
void DC2CALL Allo_Flinch(gptr o);                               /* 0x43C3C0 */
void DC2CALL Allo_Stagger(gptr o);                              /* 0x43C450 */
void DC2CALL Allo_Fall(gptr o);                                 /* 0x43C600 */
void DC2CALL Allo_DieA(gptr o);                                 /* 0x43C700 */
void DC2CALL Allo_DieB(gptr o);                                 /* 0x43C890 */
u32  DC2CALL Allo_PlAnim(gptr o);                               /* 0x43CAF0 */
u32  DC2CALL Allo_Timer(gptr o);                                /* 0x43CB10 */
void DC2CALL Allo_Tint(gptr o);                                 /* 0x43CB90 */
void DC2CALL Allo_Pick(gptr o);                                 /* 0x43CCB0 */
u32  DC2CALL Allo_FrameFar(gptr o, u32 frame, u32 dist);        /* 0x43CD00 */
u32  DC2CALL Allo_FrameNear(gptr o, u32 frame, u32 dist);       /* 0x43CD80 */
u32  DC2CALL Allo_Chance(gptr o, u32 n);                        /* 0x43CDC0 */
void DC2CALL Allo_Burn(gptr o);                                 /* 0x43CDF0 */

void DC2CALL Vec_Toward(gptr a, gptr b, u32 speed, gptr out); /* 0x48F500 */
u32  DC2CALL Water_Level(gptr pos);             /* 0x413980 */
void DC2CALL Ptera_Type(gptr o);                /* 0x429030 */
void DC2CALL Ptera_Handler(gptr o);             /* 0x429040 */
void DC2CALL Ptera_Init(gptr o);                /* 0x429290 */
void DC2CALL Ptera_Normal(gptr o);              /* 0x429430 */
void DC2CALL Ptera_Hit(gptr o);                 /* 0x429460 */
void DC2CALL Ptera_Die(gptr o);                 /* 0x429480 */
void DC2CALL Ptera_Grab(gptr o);                /* 0x4294A0 */
u32  DC2CALL Ptera_Timer(gptr o);               /* 0x4338F0 */
void DC2CALL Ptera_HoldPoint(gptr o, gptr out, gptr v); /* 0x433860 */
void DC2CALL Ptera_ThinkFly(gptr o);            /* 0x4294D0 */
void DC2CALL Ptera_Steer(gptr o);               /* 0x4297D0 */
void DC2CALL Ptera_Fly(gptr o);                 /* 0x4296A0 */
void DC2CALL Ptera_Rise(gptr o);                /* 0x429890 */
void DC2CALL Ptera_ThinkSwoop(gptr o);          /* 0x429A10 */
void DC2CALL Ptera_Swoop(gptr o);               /* 0x429C50 */
void DC2CALL Ptera_ThinkPerch(gptr o);          /* 0x429E40 */
void DC2CALL Ptera_Perch(gptr o);               /* 0x429F20 */
void DC2CALL Ptera_Peck(gptr o);                /* 0x42A070 */
void DC2CALL Ptera_Claw(gptr o);                /* 0x42A210 */
u32  DC2CALL Ptera_PlAnim(gptr o);              /* 0x42B6F0 */
void DC2CALL Ptera_Carry(gptr o);               /* 0x42A3B0 */
void DC2CALL Ptera_Dropped(gptr pl);            /* 0x42A730 */
void DC2CALL Ptera_Pin(gptr o);                 /* 0x42A810 */
void DC2CALL Ptera_Released(gptr pl);           /* 0x42AC00 */
void DC2CALL Ptera_PeckHit(gptr o);             /* 0x42AC30 */
void DC2CALL Ptera_ClawHit(gptr o);             /* 0x42ACB0 */
void DC2CALL Ptera_PinHit(gptr o);              /* 0x42AD30 */
void DC2CALL Ptera_Damage(gptr o);              /* 0x42AE50 */
void DC2CALL Ptera_Knock(gptr o);               /* 0x42B0A0 */
void DC2CALL Ptera_KnockHard(gptr o);           /* 0x42B1D0 */
void DC2CALL Ptera_KnockHeavy(gptr o);          /* 0x42B300 */
void DC2CALL Ptera_Dying(gptr o);               /* 0x42B440 */

void DC2CALL Obj_TurnFromHead(gptr o, u32 max, gptr pos); /* 0x45B3D0 */
void DC2CALL Zone_RandomPoint(gptr o, gptr out);        /* 0x487340 */
u32  DC2CALL Enemy_AttackInQuad(gptr o, gptr quad);     /* 0x479B30 */
void DC2CALL TRex_Handler(gptr o);                      /* 0x423680 */
void DC2CALL TRex_Init(gptr o);                         /* 0x423970 */
void DC2CALL TRex_Normal(gptr o);                       /* 0x423BC0 */
void DC2CALL TRex_Hit(gptr o);                          /* 0x423BF0 */
void DC2CALL TRex_Die(gptr o);                          /* 0x423C10 */
void DC2CALL TRex_Grab(gptr o);                         /* 0x423C30 */
u32  DC2CALL TRex_Timer(gptr o);                        /* 0x426320 */
u32  DC2CALL TRex_Event(gptr o);                        /* 0x426390 */
u32  DC2CALL TRex_Script(gptr o);                       /* 0x426410 */
u32  DC2CALL TRex_Target(gptr o);                       /* 0x426530 */
u32  DC2CALL TRex_Gates(gptr o);                        /* 0x4266A0 */
u32  DC2CALL TRex_Decide(gptr o);                       /* 0x4243B0 */
void DC2CALL TRex_ThinkStand(gptr o);                   /* 0x423C60 */
void DC2CALL TRex_Stand(gptr o);                        /* 0x423E40 */
void DC2CALL TRex_ThinkWalk(gptr o);                    /* 0x423F70 */
void DC2CALL TRex_Walk(gptr o);                         /* 0x4240C0 */
void DC2CALL TRex_ThinkRoar(gptr o);                    /* 0x4241F0 */
void DC2CALL TRex_Roar(gptr o);                         /* 0x424210 */
void DC2CALL TRex_ThinkLook(gptr o);                    /* 0x4242D0 */
void DC2CALL TRex_Look(gptr o);                         /* 0x424300 */
void DC2CALL TRex_ThinkSnatch(gptr o);                  /* 0x4246A0 */
void DC2CALL TRex_Snatch(gptr o);                       /* 0x4246D0 */
void DC2CALL TRex_PlSnatched(gptr pl);                  /* 0x4248A0 */
void DC2CALL TRex_Recoil(gptr o);                       /* 0x4249B0 */
void DC2CALL TRex_Burst(gptr o);                        /* 0x424A30 */
void DC2CALL TRex_Stagger(gptr o);                      /* 0x424C80 */
void DC2CALL TRex_ThinkBite(gptr o);                    /* 0x424E20 */
void DC2CALL TRex_Bite(gptr o);                         /* 0x424E70 */
void DC2CALL TRex_ThinkWalkBite(gptr o);                /* 0x424F50 */
void DC2CALL TRex_WalkBite(gptr o);                     /* 0x424F70 */
void DC2CALL TRex_ThinkCharge(gptr o);                  /* 0x425040 */
void DC2CALL TRex_Charge(gptr o);                       /* 0x425080 */
void DC2CALL TRex_Lunge(gptr o);                        /* 0x425190 */
void DC2CALL TRex_BiteHit(gptr o);                      /* 0x425240 */
void DC2CALL TRex_WalkBiteHit(gptr o);                  /* 0x425300 */
void DC2CALL TRex_LungeHit(gptr o);                     /* 0x425690 */
void DC2CALL TRex_ChargeHit(gptr o);                    /* 0x4253B0 */
void DC2CALL TRex_PlBitten(gptr pl);                    /* 0x4255D0 */
void DC2CALL TRex_Damage(gptr o);                       /* 0x425740 */
void DC2CALL TRex_Flinch(gptr o);                       /* 0x425CA0 */
void DC2CALL TRex_FlinchHard(gptr o);                   /* 0x425DC0 */
void DC2CALL TRex_Reel(gptr o);                         /* 0x425ED0 */
void DC2CALL TRex_Down(gptr o);                         /* 0x426010 */
void DC2CALL TRex_Dead(gptr o);                         /* 0x4261D0 */
void DC2CALL Plesio_Handler(gptr o);             /* 0x42B750 */
void DC2CALL Plesio_Init(gptr o);                /* 0x42B9A0 */
void DC2CALL Plesio_Normal(gptr o);              /* 0x42BBA0 */
void DC2CALL Plesio_Hit(gptr o);                 /* 0x42BBD0 */
void DC2CALL Plesio_Die(gptr o);                 /* 0x42BBF0 */
void DC2CALL Plesio_Grab(gptr o);                /* 0x42BC10 */
void DC2CALL Plesio_ThinkSwim(gptr o);           /* 0x42BC40 */
void DC2CALL Plesio_Swimming(gptr o);            /* 0x42BD10 */
void DC2CALL Plesio_ThinkTurn(gptr o);           /* 0x42BDB0 */
void DC2CALL Plesio_Turn(gptr o);                /* 0x42BDD0 */
u32 DC2CALL Plesio_Decide(gptr o);               /* 0x42BE70 */
void DC2CALL Plesio_Swim(gptr o);                /* 0x42BFF0 */
void DC2CALL Plesio_ThinkBite(gptr o);           /* 0x42C400 */
void DC2CALL Plesio_Bite(gptr o);                /* 0x42C410 */
void DC2CALL Plesio_ThinkLunge(gptr o);          /* 0x42C4A0 */
void DC2CALL Plesio_Lunge(gptr o);               /* 0x42C4B0 */
void DC2CALL Plesio_ThinkSide(gptr o);           /* 0x42C590 */
void DC2CALL Plesio_Side(gptr o);                /* 0x42C5A0 */
void DC2CALL Plesio_ThinkRise(gptr o);           /* 0x42C680 */
void DC2CALL Plesio_Rise(gptr o);                /* 0x42C690 */
void DC2CALL Plesio_BiteHit(gptr o);             /* 0x42C750 */
void DC2CALL Plesio_LungeHit(gptr o);            /* 0x42C7C0 */
void DC2CALL Plesio_RiseHit(gptr o);             /* 0x42C830 */
void DC2CALL Plesio_Damage(gptr o);              /* 0x42C890 */
void DC2CALL Plesio_Flinch(gptr o);              /* 0x42CAA0 */
void DC2CALL Plesio_Flinch2(gptr o);             /* 0x42CB00 */
void DC2CALL Plesio_Flinch3(gptr o);             /* 0x42CB60 */
void DC2CALL Plesio_Dead(gptr o);                /* 0x42CBC0 */
u32 DC2CALL Plesio_Wait(gptr o);                 /* 0x42CCA0 */
void DC2CALL Ples_TurnToPlayer(gptr o);            /* 0x42F4C0 */
u32 DC2CALL Ples_Wait(gptr o);                     /* 0x42FEA0 */
void DC2CALL Ples_PartsWater(gptr o);              /* 0x42FF10 */
u32 DC2CALL Ples11_PlayerSet(void);                /* 0x42FE80 */
void DC2CALL Ples11_Splash(gptr o, gptr d);        /* 0x42FFC0 */
void DC2CALL Ples11_Glow(gptr o, u32 mode);        /* 0x430000 */
void DC2CALL Ples11_Remove(gptr o);                /* 0x42EA20 */
void DC2CALL Ples11_Type(gptr o);                  /* 0x42DCC0 */
void DC2CALL Ples11_Handler(gptr o);               /* 0x42DCD0 */
void DC2CALL Ples11_Init(gptr o);                  /* 0x42DEA0 */
void DC2CALL Ples11_Normal(gptr o);                /* 0x42E160 */
void DC2CALL Ples11_Hit(gptr o);                   /* 0x42E190 */
void DC2CALL Ples11_Die(gptr o);                   /* 0x42E1B0 */
void DC2CALL Ples11_Grab(gptr o);                  /* 0x42E1D0 */
u32 DC2CALL Ples11_Decide(gptr o);                 /* 0x42E590 */
void DC2CALL Ples11_ThinkWait(gptr o);             /* 0x42E200 */
void DC2CALL Ples11_ThinkSwim(gptr o);             /* 0x42E330 */
void DC2CALL Ples11_ThinkBack(gptr o);             /* 0x42E470 */
void DC2CALL Ples11_Wait2(gptr o);                 /* 0x42E240 */
void DC2CALL Ples11_Swim(gptr o);                  /* 0x42E3A0 */
void DC2CALL Ples11_Back(gptr o);                  /* 0x42E4F0 */
void DC2CALL Ples11_Leap(gptr o);                  /* 0x42E840 */
void DC2CALL Ples11_Dive(gptr o);                  /* 0x42E950 */
void DC2CALL Ples11_Rise(gptr o);                  /* 0x42EA80 */
void DC2CALL Ples11_Lunge(gptr o);                 /* 0x42EB30 */
void DC2CALL Ples11_Bite(gptr o);                  /* 0x42ECA0 */
void DC2CALL Ples11_ThinkSide(gptr o);             /* 0x42ED60 */
void DC2CALL Ples11_Side(gptr o);                  /* 0x42EDD0 */
void DC2CALL Ples11_ThinkDrag(gptr o);             /* 0x42EFA0 */
void DC2CALL Ples11_PlayerUp(gptr pl);             /* 0x42F2C0 */
void DC2CALL Ples11_Drag(gptr o);                  /* 0x42EFD0 */
void DC2CALL Ples11_Eat(gptr o);                   /* 0x42F300 */
void DC2CALL Ples11_Resurface(gptr o);             /* 0x42F4E0 */
void DC2CALL Ples11_Snap(gptr o);                  /* 0x42F5F0 */
void DC2CALL Ples11_BiteHit(gptr o);               /* 0x42F6E0 */
void DC2CALL Ples11_SideHit(gptr o);               /* 0x42F770 */
void DC2CALL Ples11_ResurfaceHit(gptr o);          /* 0x42F820 */
void DC2CALL Ples11_SnapHit(gptr o);               /* 0x42F890 */
void DC2CALL Ples11_Damage(gptr o);                /* 0x42F910 */
void DC2CALL Ples11_Flinch(gptr o);                /* 0x42FBD0 */
void DC2CALL Ples11_Knock(gptr o);                 /* 0x42FC30 */
void DC2CALL Ples11_Stagger(gptr o);               /* 0x42FCA0 */
void DC2CALL Ples11_Dead(gptr o);                  /* 0x42FD90 */
void DC2CALL Ples12_Glow(gptr o, u32 mode);        /* 0x431B60 */
void DC2CALL Ples12_BiteOn(gptr o);                /* 0x431A70 */
void DC2CALL Ples12_BiteOff(gptr o);               /* 0x431AD0 */
void DC2CALL Ples12_Bubbles(gptr o);               /* 0x431EC0 */

/* the roamer (area 3 type 22): src/game/roamer.c */
void DC2CALL Obj_Vanish(gptr o);                /* 0x426C60 */
void DC2CALL Roam_Type(gptr o);                 /* 0x42CD10 */
void DC2CALL Roam_Init(gptr o);                 /* 0x42CDF0 */
void DC2CALL Roam_Start(gptr o);                /* 0x42CE60 */
void DC2CALL Roam_Exits(gptr o);                /* 0x42CFF0 */
void DC2CALL Roam_Walk(gptr o);                 /* 0x42D180 */
u32  DC2CALL Roam_Pick(gptr o, u32 n, u32 res); /* 0x42D380 */
void DC2CALL Roam_Wait(gptr o);                 /* 0x42D400 */
void DC2CALL Roam_Run(gptr o);                  /* 0x42D5C0 */
u32  DC2CALL Roam_TurnTo(gptr o, gptr max, gptr e);/* 0x42D7E0 */
void DC2CALL Roam_Normal(gptr o);               /* 0x42D820 */
void DC2CALL Roam_Bump(gptr o);                 /* 0x42D880 */
void DC2CALL Roam_Hit(gptr o);                  /* 0x42D940 */
void DC2CALL Roam_Path(gptr o);                 /* 0x42DA50 */
u32  DC2CALL Roam_Nearest(gptr o, gptr d2);     /* 0x42DC20 */

/* the giganotosaurus (type 6), src/game/gigano.c */
void DC2CALL Gig_Handler(gptr o);       /* 0x4339B0 */
void DC2CALL Gig_Init(gptr o);          /* 0x433C70 */
void DC2CALL Gig_Normal(gptr o);        /* 0x433E80 */
void DC2CALL Gig_Hit(gptr o);           /* 0x433EB0 */
void DC2CALL Gig_Die(gptr o);           /* 0x433ED0 */
void DC2CALL Gig_Grab(gptr o);          /* 0x433EF0 */
u32  DC2CALL Gig_Timer(gptr o);          /* 0x436A90 */
void DC2CALL Gig_Calm(gptr o);          /* 0x436B00 */
void DC2CALL Gig_Combo(gptr o, u32 f);  /* 0x436B80 */
u32  DC2CALL Gig_Bridge(gptr o);         /* 0x436D90 */
u32  DC2CALL Gig_OnBridge(gptr o);       /* 0x4370B0 */
u32  DC2CALL Gig_Turn(gptr o, u32 bite); /* 0x437180 */
u32  DC2CALL Gig_Decide(gptr o, u32 ff); /* 0x434400 */
void DC2CALL Gig_ThinkWalk(gptr o);     /* 0x433F20 */
void DC2CALL Gig_ThinkStand(gptr o);    /* 0x4340C0 */
void DC2CALL Gig_ThinkRun(gptr o);      /* 0x4341D0 */
void DC2CALL Gig_ThinkFace(gptr o);     /* 0x434BD0 */
void DC2CALL Gig_ThinkBite(gptr o);     /* 0x434C70 */
void DC2CALL Gig_ThinkSwipe(gptr o);    /* 0x435080 */
void DC2CALL Gig_ThinkSnatch(gptr o);   /* 0x4351B0 */
void DC2CALL Gig_ThinkMaul(gptr o);     /* 0x4353C0 */
void DC2CALL Gig_ThinkCharge(gptr o);   /* 0x435780 */
void DC2CALL Gig_Walk(gptr o);          /* 0x433F90 */
void DC2CALL Gig_Stand(gptr o);         /* 0x434120 */
void DC2CALL Gig_Run(gptr o);           /* 0x4342A0 */
void DC2CALL Gig_TurnStep(gptr o);      /* 0x434670 */
void DC2CALL Gig_Roar(gptr o);          /* 0x434730 */
void DC2CALL Gig_RoarStun(gptr pl);     /* 0x434900 */
void DC2CALL Gig_Threat(gptr o);        /* 0x434AA0 */
void DC2CALL Gig_Shake(gptr o);         /* 0x434BF0 */
void DC2CALL Gig_Bite(gptr o);          /* 0x434CB0 */
void DC2CALL Gig_Bite2(gptr o);         /* 0x434EF0 */
void DC2CALL Gig_Lunge(gptr o);         /* 0x4355D0 */
void DC2CALL Gig_Charge(gptr o);        /* 0x4357C0 */
void DC2CALL Gig_Swipe(gptr o);         /* 0x4350B0 */
void DC2CALL Gig_Snatch(gptr o);        /* 0x4351E0 */
void DC2CALL Gig_Maul(gptr o);          /* 0x4353F0 */
void DC2CALL Gig_Smash(gptr o);         /* 0x435970 */
void DC2CALL Gig_BridgeFall(gptr pl);   /* 0x435B00 */
void DC2CALL Gig_BiteHit(gptr o);       /* 0x435B60 */
void DC2CALL Gig_Bite2Hit(gptr o);      /* 0x435CC0 */
void DC2CALL Gig_SwipeHit(gptr o);      /* 0x435DB0 */
void DC2CALL Gig_MaulHit(gptr o);       /* 0x435E90 */
void DC2CALL Gig_LungeHit(gptr o);      /* 0x435EF0 */
void DC2CALL Gig_ChargeHit(gptr o);     /* 0x435FE0 */
void DC2CALL Gig_SmashHit(gptr o);      /* 0x4360D0 */
void DC2CALL Gig_Damage(gptr o);        /* 0x436140 */
void DC2CALL Gig_Flinch(gptr o);        /* 0x436740 */
void DC2CALL Gig_Stagger(gptr o);       /* 0x436860 */
void DC2CALL Gig_Down(gptr o);          /* 0x436910 */
void DC2CALL Gig_Dead(gptr o);          /* 0x4369B0 */

/* the inostrancevia (type 14), src/game/inostr.c */
void DC2CALL Ino_Handler(gptr o);       /* 0x437230 */
void DC2CALL Ino_Init(gptr o);          /* 0x437540 */
void DC2CALL Ino_Normal(gptr o);        /* 0x4376D0 */
void DC2CALL Ino_Hit(gptr o);           /* 0x437710 */
void DC2CALL Ino_Die(gptr o);           /* 0x437730 */
void DC2CALL Ino_Grab(gptr o);          /* 0x437750 */
u32  DC2CALL Ino_Side(gptr o);          /* 0x4397A0 */
u32  DC2CALL Ino_Decide(gptr o);        /* 0x437960 */
void DC2CALL Ino_ThinkWait(gptr o);     /* 0x437780 */
void DC2CALL Ino_Wait(gptr o);          /* 0x437800 */
void DC2CALL Ino_ThinkWalk(gptr o);     /* 0x437860 */
void DC2CALL Ino_Walk(gptr o);          /* 0x4378B0 */
void DC2CALL Ino_ThinkRoar(gptr o);     /* 0x437BF0 */
void DC2CALL Ino_Roar(gptr o);          /* 0x437C10 */
void DC2CALL Ino_Turn(gptr o);          /* 0x437ED0 */
void DC2CALL Ino_ThinkFall(gptr o);     /* 0x437CC0 */
void DC2CALL Ino_Fall(gptr o);          /* 0x437CE0 */
void DC2CALL Ino_ThinkBite(gptr o);     /* 0x438110 */
void DC2CALL Ino_Bite(gptr o);          /* 0x438060 */
void DC2CALL Ino_Bite2(gptr o);         /* 0x438140 */
void DC2CALL Ino_ThinkGrab(gptr o);     /* 0x4381F0 */
void DC2CALL Ino_Eat(gptr o);           /* 0x438210 */
void DC2CALL Ino_EatEnd(gptr pl);       /* 0x4385E0 */
void DC2CALL Ino_Pounce(gptr o);        /* 0x438650 */
void DC2CALL Ino_BiteHit(gptr o);       /* 0x4388F0 */
void DC2CALL Ino_Bite2Hit(gptr o);      /* 0x438970 */
void DC2CALL Ino_EatHit(gptr o);        /* 0x4389F0 */
void DC2CALL Ino_Damage(gptr o);        /* 0x438B40 */
void DC2CALL Ino_Flinch(gptr o);        /* 0x438F70 */
void DC2CALL Ino_Knock(gptr o);         /* 0x439000 */
void DC2CALL Ino_Stagger(gptr o);       /* 0x4390B0 */
void DC2CALL Ino_Down(gptr o);          /* 0x4391A0 */
void DC2CALL Ino_Hit5(gptr o);          /* 0x4392D0 */
void DC2CALL Ino_Hit6(gptr o);          /* 0x439370 */
void DC2CALL Ino_Hit7(gptr o);          /* 0x4393E0 */
void DC2CALL Ino_Hit8(gptr o);          /* 0x4394B0 */
void DC2CALL Ino_Dead(gptr o);          /* 0x439520 */
void DC2CALL Ino_DeadDown(gptr o);      /* 0x4395F0 */
void DC2CALL Ino_DeadFire(gptr o);      /* 0x4396D0 */

/* the triceratops (type 9), src/game/tricera.c */
void DC2CALL Tri_Handler(gptr o);       /* 0x43CF30 */
void DC2CALL Tri_Init(gptr o);          /* 0x43D1E0 */
void DC2CALL Tri_Normal(gptr o);        /* 0x43D3A0 */
void DC2CALL Tri_Hit(gptr o);           /* 0x43D3D0 */
void DC2CALL Tri_Die(gptr o);           /* 0x43D3F0 */
void DC2CALL Tri_Grab(gptr o);          /* 0x43D410 */
void DC2CALL Tri_ThinkRun(gptr o);      /* 0x43D440 */
void DC2CALL Tri_Run(gptr o);           /* 0x43D470 */
void DC2CALL Tri_ThinkAfter(gptr o);    /* 0x43D650 */
void DC2CALL Tri_After(gptr o);         /* 0x43D670 */
u32  DC2CALL Tri_Steer(gptr o);         /* 0x43D780 */
u32  DC2CALL Tri_Decide(gptr o);        /* 0x43DBE0 */
void DC2CALL Tri_Roar(gptr o);          /* 0x43DD00 */
void DC2CALL Tri_Swerve(gptr o);        /* 0x43DDB0 */
void DC2CALL Tri_Face(gptr o);          /* 0x43DF30 */
void DC2CALL Tri_Charge(gptr o);        /* 0x43DF50 */
void DC2CALL Tri_Gore(gptr o);          /* 0x43E070 */
void DC2CALL Tri_ChargeHit(gptr o);     /* 0x43E1B0 */
void DC2CALL Tri_GoreHit(gptr o);       /* 0x43E200 */
void DC2CALL Tri_Damage(gptr o);        /* 0x43E250 */
void DC2CALL Tri_Flinch(gptr o);        /* 0x43E6A0 */
void DC2CALL Tri_Knock(gptr o);         /* 0x43E880 */
void DC2CALL Tri_Dead(gptr o);          /* 0x43E9D0 */
void DC2CALL Tri_DeadRun(gptr o);       /* 0x43EAB0 */
u32  DC2CALL Tri_Timer(gptr o);         /* 0x43EBD0 */
void DC2CALL Tri_Recover(gptr o);       /* 0x43EC40 */
void DC2CALL Tri_Dust(gptr o);          /* 0x43ECC0 */
void DC2CALL Tri_Phase(gptr o);         /* 0x43EDD0 */
void DC2CALL Tri_Debris(gptr o, u32 side); /* 0x43F6D0 */

/* the mosasaurus (type 10), src/game/mosa.c */
void DC2CALL Mosa_Handler(gptr o);      /* 0x43F820 */
void DC2CALL Mosa_Init(gptr o);         /* 0x43FB00 */
void DC2CALL Mosa_Normal(gptr o);       /* 0x43FCA0 */
void DC2CALL Mosa_Hit(gptr o);          /* 0x43FCD0 */
void DC2CALL Mosa_Die(gptr o);          /* 0x43FCF0 */
void DC2CALL Mosa_Grab(gptr o);         /* 0x43FD10 */
void DC2CALL Mosa_ThinkSwim(gptr o);    /* 0x43FD40 */
void DC2CALL Mosa_Swim(gptr o);         /* 0x43FDB0 */
void DC2CALL Mosa_ThinkRush(gptr o);    /* 0x43FE50 */
void DC2CALL Mosa_Rush(gptr o);         /* 0x43FEB0 */
void DC2CALL Mosa_ThinkTurn(gptr o);    /* 0x43FF60 */
void DC2CALL Mosa_Turn(gptr o);         /* 0x43FFD0 */
void DC2CALL Mosa_ThinkPitch(gptr o);   /* 0x440070 */
void DC2CALL Mosa_Pitch(gptr o);        /* 0x4400E0 */
u32  DC2CALL Mosa_Decide(gptr o);       /* 0x440180 */
void DC2CALL Mosa_Dive(gptr o);         /* 0x4405A0 */
void DC2CALL Mosa_ThinkLunge(gptr o);   /* 0x4406F0 */
void DC2CALL Mosa_Lunge(gptr o);        /* 0x440710 */
void DC2CALL Mosa_Leap(gptr o);         /* 0x440880 */
void DC2CALL Mosa_PlThrown(gptr pl);    /* 0x440AF0 */
void DC2CALL Mosa_ThinkSeize(gptr o);   /* 0x440D30 */
void DC2CALL Mosa_Seize(gptr o);        /* 0x440D80 */
void DC2CALL Mosa_PlDrop(gptr pl);      /* 0x4411E0 */
void DC2CALL Mosa_LungeHit(gptr o);     /* 0x4412B0 */
void DC2CALL Mosa_SeizeHit(gptr o);     /* 0x441340 */
void DC2CALL Mosa_Damage(gptr o);       /* 0x441440 */
void DC2CALL Mosa_Flinch(gptr o);       /* 0x4416B0 */
void DC2CALL Mosa_Flinch2(gptr o);      /* 0x441720 */
void DC2CALL Mosa_HitDive(gptr o);      /* 0x4417B0 */
void DC2CALL Mosa_Knock(gptr o);        /* 0x441830 */
void DC2CALL Mosa_Knock2(gptr o);       /* 0x4418F0 */
void DC2CALL Mosa_HitDive2(gptr o);     /* 0x4419A0 */
void DC2CALL Mosa_Dead(gptr o);         /* 0x441A20 */
void DC2CALL Mosa_Dead2(gptr o);        /* 0x441BD0 */
u32  DC2CALL Mosa_Timer(gptr o);        /* 0x441D90 */
void DC2CALL Mosa_Bite(gptr o);         /* 0x441E30 */
void DC2CALL Mosa_Release(gptr o);      /* 0x441E80 */
void DC2CALL Mosa_PitchTo(gptr o, u32 max, gptr p); /* 0x441EE0 */
u32  DC2CALL Mosa_PitchDiff(gptr from, gptr p, u32 cur); /* 0x441F40 */
/* compso.c: the compsognathus (type 13) */
void DC2CALL Compso_Handler (gptr o);  /* 0x445690 */
void DC2CALL Compso_Init    (gptr o);     /* 0x445720 */
void DC2CALL Compso_Normal  (gptr o);   /* 0x4457F0 */
void DC2CALL Compso_Hit     (gptr o);      /* 0x445820 */
void DC2CALL Compso_Die     (gptr o);      /* 0x445840 */
void DC2CALL Compso_Grab    (gptr o);     /* 0x445860 */
void DC2CALL Compso_Turn    (gptr o);     /* 0x445880 */
void DC2CALL Compso_Cling   (gptr o);    /* 0x4458A0 */
void DC2CALL Compso_Damage  (gptr o);   /* 0x445910 */
void DC2CALL Compso_Hurt    (gptr o);     /* 0x445920 */
void DC2CALL Compso_Fade    (gptr o);     /* 0x445980 */
void DC2CALL Flash_Full     (gptr o);      /* 0x4459A0 */
void DC2CALL Flash_Quads    (gptr o);     /* 0x445A60 */
/* kof.c: the extra game's playable dinosaurs, shared */
u32  DC2CALL Combo_Input(gptr c, u32 pad);                         /* 0x418F40 */
void DC2CALL Kof_Super(gptr o);                                    /* 0x418FA0 */
u32  DC2CALL Kof_HitFoe(gptr m, gptr pos, u32 slot, u32 foe);      /* 0x47B9E0 */
void DC2CALL Kof_Attack(gptr o, u32 foe, u32 dmg, u32 react, u32 slot); /* 0x47CB20 */
/* kof_ino.c: the inostrancevia of the duel (types 41/42) */
void DC2CALL KIno1_Handler(gptr o);             /* 0x45D100 */
void DC2CALL KIno1_Init(gptr o);                /* 0x45D2C0 */
void DC2CALL KIno1_Normal(gptr o);              /* 0x45D410 */
void DC2CALL KIno1_Hit(gptr o);                 /* 0x45D500 */
void DC2CALL KIno1_Die(gptr o);                 /* 0x45D520 */
void DC2CALL KIno1_Attack(gptr o);              /* 0x45D540 */
void DC2CALL KIno1_PickStand(gptr o);           /* 0x45D590 */
void DC2CALL KIno1_Stand(gptr o);               /* 0x45D680 */
void DC2CALL KIno1_PickWalk(gptr o);            /* 0x45D6F0 */
void DC2CALL KIno1_Walk(gptr o);                /* 0x45D7C0 */
void DC2CALL KIno1_Turn(gptr o);                /* 0x45D830 */
void DC2CALL KIno1_PickOn(gptr o);              /* 0x45D920 */
void DC2CALL KIno1_On(gptr o);                  /* 0x45D9E0 */
void DC2CALL KIno1_PickBack(gptr o);            /* 0x45DA50 */
void DC2CALL KIno1_Back(gptr o);                /* 0x45DB10 */
void DC2CALL KIno1_Roar(gptr o);                /* 0x45DB80 */
void DC2CALL KIno1_About(gptr o);               /* 0x45DC20 */
void DC2CALL KIno1_Steer7(gptr o);              /* 0x45DD50 */
void DC2CALL KIno1_Bite(gptr o);                /* 0x45DD80 */
void DC2CALL KIno1_PickBite2(gptr o);           /* 0x45DE30 */
void DC2CALL KIno1_Bite2(gptr o);               /* 0x45DE80 */
void DC2CALL KIno1_Steer9(gptr o);              /* 0x45DF30 */
void DC2CALL KIno1_Lunge(gptr o);               /* 0x45DF60 */
void DC2CALL KIno1_Land7(gptr o);               /* 0x45E030 */
void DC2CALL KIno1_Land8(gptr o);               /* 0x45E060 */
void DC2CALL KIno1_Land9(gptr o);               /* 0x45E090 */
void DC2CALL KIno1_Hurt(gptr o);                /* 0x45E0B0 */
void DC2CALL KIno1_Flinch8(gptr o);             /* 0x45E290 */
void DC2CALL KIno1_Flinch9(gptr o);             /* 0x45E300 */
void DC2CALL KIno1_Dying(gptr o);               /* 0x45E370 */
void DC2CALL KIno2_Handler(gptr o);             /* 0x45E430 */
void DC2CALL KIno2_Init(gptr o);                /* 0x45E5F0 */
void DC2CALL KIno2_Normal(gptr o);              /* 0x45E740 */
void DC2CALL KIno2_Hit(gptr o);                 /* 0x45E830 */
void DC2CALL KIno2_Die(gptr o);                 /* 0x45E850 */
void DC2CALL KIno2_Attack(gptr o);              /* 0x45E870 */
void DC2CALL KIno2_PickStand(gptr o);           /* 0x45E8C0 */
void DC2CALL KIno2_Stand(gptr o);               /* 0x45E9B0 */
void DC2CALL KIno2_PickWalk(gptr o);            /* 0x45EA20 */
void DC2CALL KIno2_Walk(gptr o);                /* 0x45EAF0 */
void DC2CALL KIno2_Turn(gptr o);                /* 0x45EB60 */
void DC2CALL KIno2_PickOn(gptr o);              /* 0x45EC50 */
void DC2CALL KIno2_On(gptr o);                  /* 0x45ED10 */
void DC2CALL KIno2_PickBack(gptr o);            /* 0x45ED80 */
void DC2CALL KIno2_Back(gptr o);                /* 0x45EE40 */
void DC2CALL KIno2_Roar(gptr o);                /* 0x45EEB0 */
void DC2CALL KIno2_About(gptr o);               /* 0x45EF50 */
void DC2CALL KIno2_Steer7(gptr o);              /* 0x45F080 */
void DC2CALL KIno2_Bite(gptr o);                /* 0x45F0B0 */
void DC2CALL KIno2_PickBite2(gptr o);           /* 0x45F160 */
void DC2CALL KIno2_Bite2(gptr o);               /* 0x45F1B0 */
void DC2CALL KIno2_Steer9(gptr o);              /* 0x45F260 */
void DC2CALL KIno2_Lunge(gptr o);               /* 0x45F290 */
void DC2CALL KIno2_Land7(gptr o);               /* 0x45F360 */
void DC2CALL KIno2_Land8(gptr o);               /* 0x45F390 */
void DC2CALL KIno2_Land9(gptr o);               /* 0x45F3C0 */
void DC2CALL KIno2_Hurt(gptr o);                /* 0x45F3E0 */
void DC2CALL KIno2_Flinch8(gptr o);             /* 0x45F5C0 */
void DC2CALL KIno2_Flinch9(gptr o);             /* 0x45F630 */
void DC2CALL KIno2_Dying(gptr o);               /* 0x45F6A0 */
void DC2CALL KTri1_Handler(gptr o);             /* 0x463DA0 */
void DC2CALL KTri1_Init(gptr o);                /* 0x463F90 */
void DC2CALL KTri1_Normal(gptr o);              /* 0x4640F0 */
void DC2CALL KTri1_Hit(gptr o);                 /* 0x464170 */
void DC2CALL KTri1_Die(gptr o);                 /* 0x464190 */
void DC2CALL KTri1_Attack(gptr o);              /* 0x4641B0 */
void DC2CALL KTri1_PickStand(gptr o);           /* 0x464200 */
void DC2CALL KTri1_Stand(gptr o);               /* 0x4642E0 */
void DC2CALL KTri1_PickWalk(gptr o);            /* 0x464340 */
void DC2CALL KTri1_Walk(gptr o);                /* 0x464400 */
void DC2CALL KTri1_Turn(gptr o);                /* 0x464460 */
void DC2CALL KTri1_PickOn(gptr o);              /* 0x464540 */
void DC2CALL KTri1_On(gptr o);                  /* 0x4645F0 */
void DC2CALL KTri1_PickCharge(gptr o);          /* 0x464650 */
void DC2CALL KTri1_Charge(gptr o);              /* 0x464700 */
void DC2CALL KTri1_PickBack(gptr o);            /* 0x464770 */
void DC2CALL KTri1_Back(gptr o);                /* 0x464810 */
void DC2CALL KTri1_Steer6(gptr o);              /* 0x464870 */
void DC2CALL KTri1_Roar(gptr o);                /* 0x4648A0 */
void DC2CALL KTri1_Steer7(gptr o);              /* 0x464930 */
void DC2CALL KTri1_Gore(gptr o);                /* 0x464960 */
void DC2CALL KTri1_Steer8(gptr o);              /* 0x4649F0 */
void DC2CALL KTri1_Swipe(gptr o);               /* 0x464A20 */
void DC2CALL KTri1_PickSuper(gptr o);           /* 0x464AD0 */
void DC2CALL KTri1_Super(gptr o);               /* 0x464B00 */
void DC2CALL KTri1_Land4(gptr o);               /* 0x464C00 */
void DC2CALL KTri1_Land7(gptr o);               /* 0x464C80 */
void DC2CALL KTri1_Land8(gptr o);               /* 0x464C30 */
void DC2CALL KTri1_Hurt(gptr o);                /* 0x464CB0 */
void DC2CALL KTri1_Flinch8(gptr o);             /* 0x464E50 */
void DC2CALL KTri1_Flinch9(gptr o);             /* 0x464EC0 */
void DC2CALL KTri1_Knock(gptr o);               /* 0x464F30 */
void DC2CALL KTri1_Dying(gptr o);               /* 0x464FB0 */
void DC2CALL KTri2_Handler(gptr o);             /* 0x465070 */
void DC2CALL KTri2_Init(gptr o);                /* 0x465260 */
void DC2CALL KTri2_Normal(gptr o);              /* 0x4653C0 */
void DC2CALL KTri2_Hit(gptr o);                 /* 0x465440 */
void DC2CALL KTri2_Die(gptr o);                 /* 0x465460 */
void DC2CALL KTri2_Attack(gptr o);              /* 0x465480 */
void DC2CALL KTri2_PickStand(gptr o);           /* 0x4654D0 */
void DC2CALL KTri2_Stand(gptr o);               /* 0x4655B0 */
void DC2CALL KTri2_PickWalk(gptr o);            /* 0x465610 */
void DC2CALL KTri2_Walk(gptr o);                /* 0x4656D0 */
void DC2CALL KTri2_Turn(gptr o);                /* 0x465730 */
void DC2CALL KTri2_PickOn(gptr o);              /* 0x465810 */
void DC2CALL KTri2_On(gptr o);                  /* 0x4658C0 */
void DC2CALL KTri2_PickCharge(gptr o);          /* 0x465920 */
void DC2CALL KTri2_Charge(gptr o);              /* 0x4659D0 */
void DC2CALL KTri2_PickBack(gptr o);            /* 0x465A40 */
void DC2CALL KTri2_Back(gptr o);                /* 0x465AE0 */
void DC2CALL KTri2_Steer6(gptr o);              /* 0x465B40 */
void DC2CALL KTri2_Roar(gptr o);                /* 0x465B70 */
void DC2CALL KTri2_Steer7(gptr o);              /* 0x465C00 */
void DC2CALL KTri2_Gore(gptr o);                /* 0x465C30 */
void DC2CALL KTri2_Steer8(gptr o);              /* 0x465CC0 */
void DC2CALL KTri2_Swipe(gptr o);               /* 0x465CF0 */
void DC2CALL KTri2_PickSuper(gptr o);           /* 0x465DA0 */
void DC2CALL KTri2_Super(gptr o);               /* 0x465DD0 */
void DC2CALL KTri2_Land4(gptr o);               /* 0x465ED0 */
void DC2CALL KTri2_Land7(gptr o);               /* 0x465F00 */
void DC2CALL KTri2_Land8(gptr o);               /* 0x465F30 */
void DC2CALL KTri2_Hurt(gptr o);                /* 0x465F80 */
void DC2CALL KTri2_Flinch8(gptr o);             /* 0x466120 */
void DC2CALL KTri2_Flinch9(gptr o);             /* 0x466190 */
void DC2CALL KTri2_Knock(gptr o);               /* 0x466200 */
void DC2CALL KTri2_Dying(gptr o);               /* 0x466280 */
void DC2CALL KRap1_Handler(gptr o);             /* 0x456180 */
void DC2CALL KRap1_Init(gptr o);                /* 0x456240 */
void DC2CALL KRap1_Normal(gptr o);              /* 0x456370 */
void DC2CALL KRap1_HurtLight(gptr o);           /* 0x4563F0 */
void DC2CALL KRap1_HurtBack(gptr o);            /* 0x456570 */
void DC2CALL KRap1_HurtHard(gptr o);            /* 0x456720 */
void DC2CALL KRap1_Hit(gptr o);                 /* 0x4568C0 */
void DC2CALL KRap1_Stand(gptr o);               /* 0x4568E0 */
void DC2CALL KRap1_Walk(gptr o);                /* 0x456A00 */
void DC2CALL KRap1_Run(gptr o);                 /* 0x456B50 */
void DC2CALL KRap1_Back(gptr o);                /* 0x456C80 */
void DC2CALL KRap1_Attack(gptr o);              /* 0x456DA0 */
void DC2CALL KRap1_Turn(gptr o);                /* 0x456DC0 */
void DC2CALL KRap1_Spin(gptr o);                /* 0x456F00 */
void DC2CALL KRap1_Taunt(gptr o);               /* 0x456FC0 */
void DC2CALL KRap1_Face8(gptr o);               /* 0x457080 */
void DC2CALL KRap1_Face9(gptr o);               /* 0x457130 */
void DC2CALL KRap1_Claw(gptr o);                /* 0x4571E0 */
void DC2CALL KRap1_Leap(gptr o);                /* 0x4573F0 */
void DC2CALL KRap1_Kick(gptr o);                /* 0x457560 */
void DC2CALL KRap1_Bite(gptr o);                /* 0x457710 */
void DC2CALL KRap1_Super(gptr o);               /* 0x457870 */
void DC2CALL KRap_Die(gptr o);                  /* 0x458310 */
void DC2CALL KRap2_Handler(gptr o);             /* 0x457BB0 */
void DC2CALL KRap2_Init(gptr o);                /* 0x457C70 */
void DC2CALL KRap2_Normal(gptr o);              /* 0x457DA0 */
void DC2CALL KRap2_HurtLight(gptr o);           /* 0x457E20 */
void DC2CALL KRap2_HurtBack(gptr o);            /* 0x457FA0 */
void DC2CALL KRap2_HurtHard(gptr o);            /* 0x458150 */
void DC2CALL KRap2_Hit(gptr o);                 /* 0x4582F0 */
void DC2CALL KRap2_Stand(gptr o);               /* 0x458430 */
void DC2CALL KRap2_Walk(gptr o);                /* 0x458550 */
void DC2CALL KRap2_Run(gptr o);                 /* 0x4586A0 */
void DC2CALL KRap2_Back(gptr o);                /* 0x4587D0 */
void DC2CALL KRap2_Attack(gptr o);              /* 0x4588F0 */
void DC2CALL KRap2_Turn(gptr o);                /* 0x458910 */
void DC2CALL KRap2_Spin(gptr o);                /* 0x458A50 */
void DC2CALL KRap2_Taunt(gptr o);               /* 0x458B10 */
void DC2CALL KRap2_Face8(gptr o);               /* 0x458BD0 */
void DC2CALL KRap2_Face9(gptr o);               /* 0x458C80 */
void DC2CALL KRap2_Claw(gptr o);                /* 0x458D30 */
void DC2CALL KRap2_Leap(gptr o);                /* 0x458F40 */
void DC2CALL KRap2_Kick(gptr o);                /* 0x4590B0 */
void DC2CALL KRap2_Bite(gptr o);                /* 0x459260 */
void DC2CALL KRap2_Super(gptr o);               /* 0x4593C0 */
void DC2CALL KOvi1_Handler(gptr o);             /* 0x466340 */
void DC2CALL KOvi1_Init(gptr o);                /* 0x466400 */
void DC2CALL KOvi1_Normal(gptr o);              /* 0x466530 */
void DC2CALL KOvi1_HurtLight(gptr o);           /* 0x4665B0 */
void DC2CALL KOvi1_HurtBack(gptr o);            /* 0x466730 */
void DC2CALL KOvi1_HurtHard(gptr o);            /* 0x4668F0 */
void DC2CALL KOvi1_Hit(gptr o);                 /* 0x466AB0 */
void DC2CALL KOvi1_Stand(gptr o);               /* 0x466AD0 */
void DC2CALL KOvi1_Walk(gptr o);                /* 0x466BE0 */
void DC2CALL KOvi1_Run(gptr o);                 /* 0x466D30 */
void DC2CALL KOvi1_Back(gptr o);                /* 0x466E60 */
void DC2CALL KOvi1_Attack(gptr o);              /* 0x467010 */
void DC2CALL KOvi1_Turn(gptr o);                /* 0x467030 */
void DC2CALL KOvi1_Spin(gptr o);                /* 0x467160 */
void DC2CALL KOvi1_Taunt(gptr o);               /* 0x467230 */
void DC2CALL KOvi1_Face8(gptr o);               /* 0x4672B0 */
void DC2CALL KOvi1_Face9(gptr o);               /* 0x467360 */
void DC2CALL KOvi1_Claw(gptr o);                /* 0x467410 */
void DC2CALL KOvi1_Leap(gptr o);                /* 0x4675B0 */
void DC2CALL KOvi1_Butt(gptr o);                /* 0x4676F0 */
void DC2CALL KOvi1_Kick(gptr o);                /* 0x467820 */
void DC2CALL KOvi1_Super(gptr o);               /* 0x467960 */
void DC2CALL KOvi_Die(gptr o);                  /* 0x468210 */
void DC2CALL KOvi2_Handler(gptr o);             /* 0x467A80 */
void DC2CALL KOvi2_Init(gptr o);                /* 0x467B40 */
void DC2CALL KOvi2_Normal(gptr o);              /* 0x467C70 */
void DC2CALL KOvi2_HurtLight(gptr o);           /* 0x467CF0 */
void DC2CALL KOvi2_HurtBack(gptr o);            /* 0x467E70 */
void DC2CALL KOvi2_HurtHard(gptr o);            /* 0x468030 */
void DC2CALL KOvi2_Hit(gptr o);                 /* 0x4681F0 */
void DC2CALL KOvi2_Stand(gptr o);               /* 0x468340 */
void DC2CALL KOvi2_Walk(gptr o);                /* 0x468450 */
void DC2CALL KOvi2_Run(gptr o);                 /* 0x4685A0 */
void DC2CALL KOvi2_Back(gptr o);                /* 0x4686D0 */
void DC2CALL KOvi2_Attack(gptr o);              /* 0x468880 */
void DC2CALL KOvi2_Turn(gptr o);                /* 0x4688A0 */
void DC2CALL KOvi2_Spin(gptr o);                /* 0x4689D0 */
void DC2CALL KOvi2_Taunt(gptr o);               /* 0x468AA0 */
void DC2CALL KOvi2_Face8(gptr o);               /* 0x468B20 */
void DC2CALL KOvi2_Face9(gptr o);               /* 0x468BD0 */
void DC2CALL KOvi2_Claw(gptr o);                /* 0x468C80 */
void DC2CALL KOvi2_Leap(gptr o);                /* 0x468E20 */
void DC2CALL KOvi2_Butt(gptr o);                /* 0x468F60 */
void DC2CALL KOvi2_Kick(gptr o);                /* 0x469090 */
void DC2CALL KOvi2_Super(gptr o);               /* 0x4691D0 */
void DC2CALL KAllo1_Handler(gptr o);             /* 0x45F760 */
void DC2CALL KAllo1_Init(gptr o);                /* 0x45F940 */
void DC2CALL KAllo1_Normal(gptr o);              /* 0x45FAA0 */
void DC2CALL KAllo1_Hit(gptr o);                 /* 0x45FB30 */
void DC2CALL KAllo1_Die(gptr o);                 /* 0x45FB50 */
void DC2CALL KAllo1_Attack(gptr o);              /* 0x45FB70 */
void DC2CALL KAllo1_PickStand(gptr o);           /* 0x45FBC0 */
void DC2CALL KAllo1_Stand(gptr o);               /* 0x45FCF0 */
void DC2CALL KAllo1_PickWalk(gptr o);            /* 0x45FD60 */
void DC2CALL KAllo1_Walk(gptr o);                /* 0x45FE80 */
void DC2CALL KAllo1_Turn(gptr o);                /* 0x45FEF0 */
void DC2CALL KAllo1_PickOn(gptr o);              /* 0x45FFE0 */
void DC2CALL KAllo1_On(gptr o);                  /* 0x4600E0 */
void DC2CALL KAllo1_PickRun(gptr o);             /* 0x460180 */
void DC2CALL KAllo1_Run(gptr o);                 /* 0x460260 */
void DC2CALL KAllo1_PickBack(gptr o);            /* 0x460300 */
void DC2CALL KAllo1_Back(gptr o);                /* 0x4603D0 */
void DC2CALL KAllo1_About(gptr o);               /* 0x460470 */
void DC2CALL KAllo1_PickFace(gptr o);            /* 0x4605D0 */
void DC2CALL KAllo1_Face(gptr o);                /* 0x460710 */
void DC2CALL KAllo1_PickStep(gptr o);            /* 0x460860 */
void DC2CALL KAllo1_Step(gptr o);                /* 0x460910 */
void DC2CALL KAllo1_Roar9(gptr o);               /* 0x460A10 */
void DC2CALL KAllo1_Rage(gptr o);                /* 0x460AA0 */
void DC2CALL KAllo1_Roar(gptr o);                /* 0x460B30 */
void DC2CALL KAllo1_PickBite(gptr o);            /* 0x460BD0 */
void DC2CALL KAllo1_Bite(gptr o);                /* 0x460C20 */
void DC2CALL KAllo1_Steer13(gptr o);             /* 0x460D50 */
void DC2CALL KAllo1_Charge(gptr o);              /* 0x460D80 */
void DC2CALL KAllo1_Steer14(gptr o);             /* 0x460E70 */
void DC2CALL KAllo1_Tail(gptr o);                /* 0x460EA0 */
void DC2CALL KAllo1_Sweep(gptr o);               /* 0x460F40 */
void DC2CALL KAllo1_LandBite(gptr o);            /* 0x461000 */
void DC2CALL KAllo1_LandTail(gptr o);            /* 0x461030 */
void DC2CALL KAllo1_LandSweep(gptr o);           /* 0x461050 */
void DC2CALL KAllo1_Hurt(gptr o);                /* 0x461080 */
void DC2CALL KAllo1_Flinch8(gptr o);             /* 0x461310 */
void DC2CALL KAllo1_Flinch9(gptr o);             /* 0x461390 */
void DC2CALL KAllo1_Thrown(gptr o);              /* 0x461410 */
void DC2CALL KAllo1_Knocked(gptr o);             /* 0x461530 */
void DC2CALL KAllo1_Dying(gptr o);               /* 0x4615A0 */
void DC2CALL KAllo1_DyingSide(gptr o);           /* 0x461700 */
void DC2CALL KAllo1_Glow(gptr o);                /* 0x461920 */
u32  DC2CALL KAllo1_Cut(gptr o, u32 frame);      /* 0x461A50 */
void DC2CALL KAllo2_Handler(gptr o);             /* 0x461A80 */
void DC2CALL KAllo2_Init(gptr o);                /* 0x461C60 */
void DC2CALL KAllo2_Normal(gptr o);              /* 0x461DC0 */
void DC2CALL KAllo2_Hit(gptr o);                 /* 0x461E50 */
void DC2CALL KAllo2_Die(gptr o);                 /* 0x461E70 */
void DC2CALL KAllo2_Attack(gptr o);              /* 0x461E90 */
void DC2CALL KAllo2_PickStand(gptr o);           /* 0x461EE0 */
void DC2CALL KAllo2_Stand(gptr o);               /* 0x462010 */
void DC2CALL KAllo2_PickWalk(gptr o);            /* 0x462080 */
void DC2CALL KAllo2_Walk(gptr o);                /* 0x4621A0 */
void DC2CALL KAllo2_Turn(gptr o);                /* 0x462210 */
void DC2CALL KAllo2_PickOn(gptr o);              /* 0x462300 */
void DC2CALL KAllo2_On(gptr o);                  /* 0x462400 */
void DC2CALL KAllo2_PickRun(gptr o);             /* 0x4624A0 */
void DC2CALL KAllo2_Run(gptr o);                 /* 0x462580 */
void DC2CALL KAllo2_PickBack(gptr o);            /* 0x462620 */
void DC2CALL KAllo2_Back(gptr o);                /* 0x4626F0 */
void DC2CALL KAllo2_About(gptr o);               /* 0x462790 */
void DC2CALL KAllo2_PickFace(gptr o);            /* 0x4628F0 */
void DC2CALL KAllo2_Face(gptr o);                /* 0x462A30 */
void DC2CALL KAllo2_PickStep(gptr o);            /* 0x462B80 */
void DC2CALL KAllo2_Step(gptr o);                /* 0x462C30 */
void DC2CALL KAllo2_Roar9(gptr o);               /* 0x462D30 */
void DC2CALL KAllo2_Rage(gptr o);                /* 0x462DC0 */
void DC2CALL KAllo2_Roar(gptr o);                /* 0x462E50 */
void DC2CALL KAllo2_PickBite(gptr o);            /* 0x462EF0 */
void DC2CALL KAllo2_Bite(gptr o);                /* 0x462F40 */
void DC2CALL KAllo2_Steer13(gptr o);             /* 0x463070 */
void DC2CALL KAllo2_Charge(gptr o);              /* 0x4630A0 */
void DC2CALL KAllo2_Steer14(gptr o);             /* 0x463190 */
void DC2CALL KAllo2_Tail(gptr o);                /* 0x4631C0 */
void DC2CALL KAllo2_Sweep(gptr o);               /* 0x463260 */
void DC2CALL KAllo2_LandBite(gptr o);            /* 0x463320 */
void DC2CALL KAllo2_LandTail(gptr o);            /* 0x463350 */
void DC2CALL KAllo2_LandSweep(gptr o);           /* 0x463370 */
void DC2CALL KAllo2_Hurt(gptr o);                /* 0x4633A0 */
void DC2CALL KAllo2_Flinch8(gptr o);             /* 0x463630 */
void DC2CALL KAllo2_Flinch9(gptr o);             /* 0x4636B0 */
void DC2CALL KAllo2_Thrown(gptr o);              /* 0x463730 */
void DC2CALL KAllo2_Knocked(gptr o);             /* 0x463850 */
void DC2CALL KAllo2_Dying(gptr o);               /* 0x4638C0 */
void DC2CALL KAllo2_DyingSide(gptr o);           /* 0x463A20 */
void DC2CALL KAllo2_Glow(gptr o);                /* 0x463C40 */
u32  DC2CALL KAllo2_Cut(gptr o, u32 frame);      /* 0x463D70 */
void DC2CALL KRex1_Handler(gptr o);       /* 0x459700 */
void DC2CALL KRex1_Init(gptr o);          /* 0x459990 */
void DC2CALL KRex1_Normal(gptr o);        /* 0x459AF0 */
void DC2CALL KRex1_Hit(gptr o);           /* 0x459B70 */
void DC2CALL KRex1_Die(gptr o);           /* 0x459B90 */
void DC2CALL KRex1_Attack(gptr o);        /* 0x459BB0 */
void DC2CALL KRex1_PickStand(gptr o);     /* 0x459C80 */
void DC2CALL KRex1_Stand(gptr o);         /* 0x459D90 */
void DC2CALL KRex1_PickWalk(gptr o);      /* 0x459E20 */
void DC2CALL KRex1_Walk(gptr o);          /* 0x459F10 */
void DC2CALL KRex1_Turn(gptr o);          /* 0x459F80 */
void DC2CALL KRex1_PickOn(gptr o);        /* 0x45A060 */
void DC2CALL KRex1_On(gptr o);            /* 0x45A130 */
void DC2CALL KRex1_PickRun(gptr o);       /* 0x45A210 */
void DC2CALL KRex1_Run(gptr o);           /* 0x45A2C0 */
void DC2CALL KRex1_PickBack(gptr o);      /* 0x45A3A0 */
void DC2CALL KRex1_Back(gptr o);          /* 0x45A450 */
void DC2CALL KRex1_Steer6(gptr o);        /* 0x45A530 */
void DC2CALL KRex1_Super(gptr o);         /* 0x45A560 */
void DC2CALL KRex1_Roar(gptr o);          /* 0x45A650 */
void DC2CALL KRex1_LongRoar(gptr o);      /* 0x45A6C0 */
void DC2CALL KRex1_Steer9(gptr o);        /* 0x45A860 */
void DC2CALL KRex1_Bite(gptr o);          /* 0x45A890 */
void DC2CALL KRex1_Steer10(gptr o);       /* 0x45A970 */
void DC2CALL KRex1_Swing(gptr o);         /* 0x45A9A0 */
void DC2CALL KRex1_Steer11(gptr o);       /* 0x45AA80 */
void DC2CALL KRex1_Tail(gptr o);          /* 0x45AAB0 */
void DC2CALL KRex1_LandBite(gptr o);      /* 0x45AB50 */
void DC2CALL KRex1_LandSwing(gptr o);     /* 0x45ABA0 */
void DC2CALL KRex1_LandTail(gptr o);      /* 0x45ABF0 */
void DC2CALL KRex1_LandRun(gptr o);       /* 0x45AC40 */
void DC2CALL KRex1_Hurt(gptr o);          /* 0x45AC60 */
void DC2CALL KRex1_Flinch8(gptr o);       /* 0x45AED0 */
void DC2CALL KRex1_Flinch9(gptr o);       /* 0x45AFE0 */
void DC2CALL KRex1_Stagger(gptr o);       /* 0x45B0F0 */
void DC2CALL KRex1_Dying(gptr o);         /* 0x45B270 */
void DC2CALL KRex2_Handler(gptr o);       /* 0x45B430 */
void DC2CALL KRex2_Init(gptr o);          /* 0x45B6C0 */
void DC2CALL KRex2_Normal(gptr o);        /* 0x45B820 */
void DC2CALL KRex2_Hit(gptr o);           /* 0x45B8A0 */
void DC2CALL KRex2_Die(gptr o);           /* 0x45B8C0 */
void DC2CALL KRex2_Attack(gptr o);        /* 0x45B8E0 */
void DC2CALL KRex2_PickStand(gptr o);     /* 0x45B9B0 */
void DC2CALL KRex2_Stand(gptr o);         /* 0x45BAC0 */
void DC2CALL KRex2_PickWalk(gptr o);      /* 0x45BB50 */
void DC2CALL KRex2_Walk(gptr o);          /* 0x45BC40 */
void DC2CALL KRex2_Turn(gptr o);          /* 0x45BCB0 */
void DC2CALL KRex2_PickOn(gptr o);        /* 0x45BD90 */
void DC2CALL KRex2_On(gptr o);            /* 0x45BE60 */
void DC2CALL KRex2_PickRun(gptr o);       /* 0x45BF40 */
void DC2CALL KRex2_Run(gptr o);           /* 0x45BFF0 */
void DC2CALL KRex2_PickBack(gptr o);      /* 0x45C0D0 */
void DC2CALL KRex2_Back(gptr o);          /* 0x45C180 */
void DC2CALL KRex2_Steer6(gptr o);        /* 0x45C260 */
void DC2CALL KRex2_Super(gptr o);         /* 0x45C290 */
void DC2CALL KRex2_Roar(gptr o);          /* 0x45C380 */
void DC2CALL KRex2_LongRoar(gptr o);      /* 0x45C3F0 */
void DC2CALL KRex2_Steer9(gptr o);        /* 0x45C590 */
void DC2CALL KRex2_Bite(gptr o);          /* 0x45C5C0 */
void DC2CALL KRex2_Steer10(gptr o);       /* 0x45C6A0 */
void DC2CALL KRex2_Swing(gptr o);         /* 0x45C6D0 */
void DC2CALL KRex2_Steer11(gptr o);       /* 0x45C7B0 */
void DC2CALL KRex2_Tail(gptr o);          /* 0x45C7E0 */
void DC2CALL KRex2_LandBite(gptr o);      /* 0x45C880 */
void DC2CALL KRex2_LandSwing(gptr o);     /* 0x45C8D0 */
void DC2CALL KRex2_LandTail(gptr o);      /* 0x45C920 */
void DC2CALL KRex2_LandRun(gptr o);       /* 0x45C970 */
void DC2CALL KRex2_Hurt(gptr o);          /* 0x45C990 */
void DC2CALL KRex2_Flinch8(gptr o);       /* 0x45CC00 */
void DC2CALL KRex2_Flinch9(gptr o);       /* 0x45CD10 */
void DC2CALL KRex2_Stagger(gptr o);       /* 0x45CE20 */
void DC2CALL KRex2_Dying(gptr o);         /* 0x45CFA0 */
void DC2CALL KComp1_Handler(gptr o);       /* 0x4692F0 */
void DC2CALL KComp1_Init(gptr o);          /* 0x469500 */
void DC2CALL KComp1_Normal(gptr o);        /* 0x469650 */
void DC2CALL KComp1_Hit(gptr o);           /* 0x469710 */
void DC2CALL KComp1_Die(gptr o);           /* 0x469730 */
void DC2CALL KComp1_Attack(gptr o);        /* 0x469750 */
void DC2CALL KComp1_PickStand(gptr o);     /* 0x4697A0 */
void DC2CALL KComp1_Stand(gptr o);         /* 0x469880 */
void DC2CALL KComp1_PickWalk(gptr o);      /* 0x469900 */
void DC2CALL KComp1_Walk(gptr o);          /* 0x4699C0 */
void DC2CALL KComp1_Turn(gptr o);          /* 0x469A30 */
void DC2CALL KComp1_PickOn(gptr o);        /* 0x469B20 */
void DC2CALL KComp1_On(gptr o);            /* 0x469BD0 */
void DC2CALL KComp1_PickRun(gptr o);       /* 0x469C30 */
void DC2CALL KComp1_Run(gptr o);           /* 0x469CE0 */
void DC2CALL KComp1_PickBack(gptr o);      /* 0x469D40 */
void DC2CALL KComp1_Back(gptr o);          /* 0x469DE0 */
void DC2CALL KComp1_Spin(gptr o);          /* 0x469E40 */
void DC2CALL KComp1_Call(gptr o);          /* 0x469EF0 */
void DC2CALL KComp1_Steer8(gptr o);        /* 0x469F70 */
void DC2CALL KComp1_Leap(gptr o);          /* 0x469FA0 */
void DC2CALL KComp1_LandLeap(gptr o);      /* 0x46A030 */
void DC2CALL KComp1_Hurt(gptr o);          /* 0x46A060 */
void DC2CALL KComp1_Flinch(gptr o);        /* 0x46A1B0 */
void DC2CALL KComp1_Dying(gptr o);         /* 0x46A250 */
void DC2CALL KComp1_PackHandler(gptr o);   /* 0x46A300 */
void DC2CALL KComp1_PackInit(gptr o);      /* 0x46A360 */
void DC2CALL KComp1_PackRun(gptr o);       /* 0x46A440 */
void DC2CALL KComp1_PackSpawn(gptr o);     /* 0x46A510 */
void DC2CALL KComp2_Handler(gptr o);       /* 0x46A5C0 */
void DC2CALL KComp2_Init(gptr o);          /* 0x46A7D0 */
void DC2CALL KComp2_Normal(gptr o);        /* 0x46A920 */
void DC2CALL KComp2_Hit(gptr o);           /* 0x46A9E0 */
void DC2CALL KComp2_Die(gptr o);           /* 0x46AA00 */
void DC2CALL KComp2_Attack(gptr o);        /* 0x46AA20 */
void DC2CALL KComp2_PickStand(gptr o);     /* 0x46AA70 */
void DC2CALL KComp2_Stand(gptr o);         /* 0x46AB50 */
void DC2CALL KComp2_PickWalk(gptr o);      /* 0x46ABD0 */
void DC2CALL KComp2_Walk(gptr o);          /* 0x46AC90 */
void DC2CALL KComp2_Turn(gptr o);          /* 0x46AD00 */
void DC2CALL KComp2_PickOn(gptr o);        /* 0x46ADF0 */
void DC2CALL KComp2_On(gptr o);            /* 0x46AEA0 */
void DC2CALL KComp2_PickRun(gptr o);       /* 0x46AF00 */
void DC2CALL KComp2_Run(gptr o);           /* 0x46AFB0 */
void DC2CALL KComp2_PickBack(gptr o);      /* 0x46B010 */
void DC2CALL KComp2_Back(gptr o);          /* 0x46B0B0 */
void DC2CALL KComp2_Spin(gptr o);          /* 0x46B110 */
void DC2CALL KComp2_Call(gptr o);          /* 0x46B1C0 */
void DC2CALL KComp2_Steer8(gptr o);        /* 0x46B240 */
void DC2CALL KComp2_Leap(gptr o);          /* 0x46B270 */
void DC2CALL KComp2_LandLeap(gptr o);      /* 0x46B300 */
void DC2CALL KComp2_Hurt(gptr o);          /* 0x46B330 */
void DC2CALL KComp2_Flinch(gptr o);        /* 0x46B480 */
void DC2CALL KComp2_Dying(gptr o);         /* 0x46B520 */
void DC2CALL KComp2_PackHandler(gptr o);   /* 0x46B5D0 */
void DC2CALL KComp2_PackInit(gptr o);      /* 0x46B630 */
void DC2CALL KComp2_PackRun(gptr o);       /* 0x46B710 */
void DC2CALL KComp2_PackSpawn(gptr o);     /* 0x46B7E0 */
void DC2CALL Drop_Handler(gptr o);  /* 0x426C70 */
void DC2CALL Drop_Init(gptr o);     /* 0x426710 */
void DC2CALL Drop_Normal(gptr o);   /* 0x426C40 */
void DC2CALL Drop_Wait(gptr o);     /* 0x426800 */
void DC2CALL Drop_Move(gptr o);     /* 0x4269A0 */
void DC2CALL Drop_MoveOn(gptr o);   /* 0x426C00 */
void DC2CALL Shell_Handler(gptr o); /* 0x426E50 */
void DC2CALL Shell_Init(gptr o);    /* 0x426D10 */
void DC2CALL Shell_Fly(gptr o);     /* 0x426DA0 */
void DC2CALL Shell_Flash(gptr o);   /* 0x426E00 */
void DC2CALL Mover_Handler(gptr o); /* 0x4275B0 */
void DC2CALL Mover_Init(gptr o);    /* 0x426EF0 */
void DC2CALL Mover_Normal(gptr o);  /* 0x427540 */
void DC2CALL Mover_Drop(gptr o);    /* 0x427570 */
void DC2CALL Mover_Fly(gptr o);     /* 0x427590 */
void DC2CALL Mover_Wait(gptr o);    /* 0x426FA0 */
void DC2CALL Mover_Move(gptr o);    /* 0x427060 */
void DC2CALL Mover_MoveOn(gptr o);  /* 0x4272C0 */
u32  DC2CALL Mover_Pitch(gptr a, gptr b, u32 yaw); /* 0x4273E0 */
void DC2CALL Mover_ToFirst(gptr o); /* 0x427300 */
void DC2CALL Mover_ToSecond(gptr o); /* 0x427490 */
void DC2CALL Bird_Handler(gptr o);  /* 0x427A50 */
void DC2CALL Bird_Init(gptr o);     /* 0x427670 */
void DC2CALL Bird_Normal(gptr o);   /* 0x427A30 */
void DC2CALL Bird_Sit(gptr o);      /* 0x4276D0 */
void DC2CALL Bird_ToFirst(gptr o);  /* 0x427800 */
void DC2CALL Bird_ToSecond(gptr o); /* 0x427920 */
void DC2CALL Rock_Handler(gptr o);  /* 0x427AD0 */
void DC2CALL Rock_Init(gptr o);     /* 0x427B40 */
void DC2CALL Rock_Wait(gptr o);     /* 0x427C00 */
void DC2CALL Rock_Hit(gptr o);      /* 0x427C20 */
u32  DC2CALL Angle_TurnMax(gptr a, gptr b, u32 cur, u32 max); /* 0x44B430 */
void DC2CALL Bug_Handler(gptr o);   /* 0x428230 */
void DC2CALL Bug_Init(gptr o);      /* 0x428310 */
void DC2CALL Bug_Chase(gptr o);     /* 0x4283F0 */
void DC2CALL Bug_Land(gptr o);      /* 0x428520 */
void DC2CALL Bug_Leave(gptr o);     /* 0x4285D0 */
void DC2CALL Bug_Die(gptr o);       /* 0x428650 */
void DC2CALL Bug_Fly(gptr o, gptr t); /* 0x428790 */
void DC2CALL Plant_Handler(gptr o); /* 0x428840 */
void DC2CALL Plant_Init(gptr o);    /* 0x4288A0 */
void DC2CALL Plant_Setup(gptr o);   /* 0x4288D0 */
void DC2CALL Plant_Hidden(gptr o);  /* 0x428A10 */
void DC2CALL Plant_Rise(gptr o);    /* 0x428B60 */
void DC2CALL Plant_Snap(gptr o);    /* 0x428BF0 */
void DC2CALL Plant_Sink(gptr o);    /* 0x428D70 */
void DC2CALL Plant_Normal(gptr o);  /* 0x428DA0 */
void DC2CALL Plant_Hit(gptr o);     /* 0x428DC0 */
void DC2CALL Plant_SetFrames(gptr o, gptr f); /* 0x428F20 */
void DC2CALL Plant_Matrix(gptr o);  /* 0x428F30 */
void DC2CALL Light_Step(gptr l);    /* 0x413270 */
void DC2CALL Burner_Handler(gptr o); /* 0x428080 */
void DC2CALL Burner_Init(gptr o);   /* 0x427C60 */
void DC2CALL Burner_Normal(gptr o); /* 0x427F10 */
void DC2CALL Burner_Off(gptr o);    /* 0x427C90 */
void DC2CALL Burner_Burn(gptr o);   /* 0x427D70 */
void DC2CALL Burner_Cool(gptr o);   /* 0x427E70 */
void DC2CALL Burner_Show(gptr o);   /* 0x427F30 */
void DC2CALL WeaponObj_Type(gptr o); /* 0x4488C0 */
void DC2CALL SubWeaponObj_Type(gptr o); /* 0x4488E0 */
void DC2CALL Pt2_Type(gptr o);      /* 0x431F70 */
void DC2CALL Pt2_Handler(gptr o);   /* 0x431F80 */
void DC2CALL Pt2_Init(gptr o);      /* 0x432240 */
void DC2CALL Pt2_Normal(gptr o);    /* 0x4323E0 */
void DC2CALL Pt2_Hit(gptr o);       /* 0x432410 */
void DC2CALL Pt2_Die(gptr o);       /* 0x432430 */
void DC2CALL Pt2_Grab(gptr o);      /* 0x432450 */
void DC2CALL Pt2_ThinkFly(gptr o);  /* 0x432480 */
void DC2CALL Pt2_ThinkSwoop(gptr o); /* 0x432A10 */
void DC2CALL Pt2_ThinkPerch(gptr o); /* 0x432D00 */
void DC2CALL Pt2_Steer(gptr o);     /* 0x4327D0 */
void DC2CALL Pt2_Fly(gptr o);       /* 0x432680 */
void DC2CALL Pt2_Rise(gptr o);      /* 0x432890 */
void DC2CALL Pt2_Swoop(gptr o);     /* 0x432B40 */
void DC2CALL Pt2_Perch(gptr o);     /* 0x432D30 */
void DC2CALL Pt2_Peck(gptr o);      /* 0x432E40 */
void DC2CALL Pt2_Claw(gptr o);      /* 0x432FD0 */
void DC2CALL Pt2_PeckHit(gptr o);   /* 0x433160 */
void DC2CALL Pt2_ClawHit(gptr o);   /* 0x4331D0 */
void DC2CALL Pt2_Damage(gptr o);    /* 0x433240 */
void DC2CALL Pt2_Knock(gptr o);     /* 0x4333A0 */
void DC2CALL Pt2_KnockHard(gptr o); /* 0x4334D0 */
void DC2CALL Pt2_Dying(gptr o);     /* 0x433600 */
void DC2CALL Ao20_Handler(gptr o);  /* 0x490670 */
void DC2CALL Ao20_Init(gptr o);     /* 0x490290 */
void DC2CALL Ao20_Normal(gptr o);   /* 0x490630 */
u32  DC2CALL Ao20_Turn(gptr o, u32 max, gptr p); /* 0x490450 */
void DC2CALL Ao20_Watch(gptr o);    /* 0x490300 */
void DC2CALL Ao20_Home(gptr o);     /* 0x490490 */
void DC2CALL Ao20_Away(gptr o);     /* 0x490570 */
void DC2CALL Ao25_Handler(gptr o);  /* 0x4912F0 */
void DC2CALL Ao25_Zone(gptr o);     /* 0x491450 */
u32  DC2CALL Ao25_Other(gptr o);    /* 0x490B40 */
u32  DC2CALL Ao25_Turn(gptr o, u32 max, gptr p, u32 mode); /* 0x490B90 */
void DC2CALL Ao25_Start(gptr o);    /* 0x490880 */
void DC2CALL Ao25_Init(gptr o);     /* 0x490720 */
void DC2CALL Ao25_Wait(gptr o);     /* 0x490810 */
void DC2CALL Ao25_Follow(gptr o);   /* 0x4908A0 */
void DC2CALL Ao25_Walk(gptr o);     /* 0x490BF0 */
void DC2CALL Ao25_Fright(gptr o);   /* 0x490DC0 */
void DC2CALL Ao25_Cower(gptr o);    /* 0x490E10 */
void DC2CALL Ao25_Drop(gptr o);     /* 0x490E60 */
void DC2CALL Ao25_Blocked(gptr o);  /* 0x490F00 */
void DC2CALL Ao25_Normal(gptr o);   /* 0x490FB0 */
void DC2CALL Ao25_Hit(gptr o);      /* 0x490FE0 */
void DC2CALL Ao25_Die(gptr o);      /* 0x4911C0 */
void DC2CALL Ao28_Handler(gptr o);  /* 0x4918D0 */
void DC2CALL Ao28_Init(gptr o);     /* 0x491510 */
void DC2CALL Ao28_Normal(gptr o);   /* 0x491750 */
void DC2CALL Ao28_Stay(gptr o);     /* 0x4915B0 */
void DC2CALL Ao28_Back(gptr o);     /* 0x491680 */
void DC2CALL Ao28_Hit(gptr o);      /* 0x491840 */
void DC2CALL Ao28_Hurt(gptr o);     /* 0x491770 */
void DC2CALL Ao28_Die(gptr o);      /* 0x491860 */
void DC2CALL K2_EShot(gptr k);      /* 0x449930 */
void DC2CALL K2_PShot(gptr k);      /* 0x449BB0 */
void DC2CALL K2Shot_Init(gptr k);   /* 0x449BF0 */
void DC2CALL K2EShot_Move(gptr k);  /* 0x449970 */
void DC2CALL K2PShot_Move(gptr k);  /* 0x449CA0 */
void DC2CALL K2_Flare(gptr k);      /* 0x449DB0 */
void DC2CALL K2Flare_Init(gptr k);  /* 0x449DF0 */
void DC2CALL K2Flare_Grow(gptr k);  /* 0x449F00 */
void DC2CALL K2Flare_Wait(gptr k);  /* 0x449F70 */
void DC2CALL K2_Swing(gptr k);      /* 0x44B2C0 */
void DC2CALL K2Swing_Init(gptr k);  /* 0x44B2F0 */
void DC2CALL K2Swing_Move(gptr k);  /* 0x44B320 */
void DC2CALL K2_Fume(gptr k);       /* 0x44BFA0 */
void DC2CALL K2Fume_Init(gptr k);   /* 0x44C000 */
void DC2CALL K2_Gone(gptr k);       /* 0x44C0E0 */
void DC2CALL K2_Flare17(gptr k);    /* 0x44C420 */
void DC2CALL K2_FlareShot(gptr k);  /* 0x44C2D0 */
void DC2CALL K2FlareShot_Init(gptr k); /* 0x44C0F0 */
void DC2CALL K2FlareShot_Fly(gptr k); /* 0x44C130 */
void DC2CALL K2FlareShot_Light(gptr k, u32 big); /* 0x44C260 */
void DC2CALL K2_FlareGlow(gptr k);  /* 0x44C300 */
void DC2CALL K2_Burst21(gptr k);    /* 0x44C450 */
void DC2CALL K2Burst21_Init(gptr k); /* 0x44C490 */
void DC2CALL K2Burst21_Looks(gptr k); /* 0x44C620 */
void DC2CALL K2_Drift(gptr k);      /* 0x44D190 */
void DC2CALL K2_Splash19(gptr k);   /* 0x44C670 */
void DC2CALL K2Splash19_Init(gptr k); /* 0x44C6B0 */
void DC2CALL K2Splash19_Rise(gptr k); /* 0x44C7B0 */
void DC2CALL K2Splash19_Spread(gptr k); /* 0x44C810 */
void DC2CALL K2Drop_Init(gptr k);   /* 0x44CA80 */
void DC2CALL K2Drop_Fall(gptr k);   /* 0x44C920 */
void DC2CALL K2_Drops8(gptr k);     /* 0x44C8C0 */
void DC2CALL K2_Drops9(gptr k);     /* 0x44CA00 */
void DC2CALL K2_Burst6(gptr k);     /* 0x44CE80 */
void DC2CALL K2Burst6_Init(gptr k); /* 0x44CEC0 */
void DC2CALL K2Burst6_Looks(gptr k); /* 0x44D050 */
void DC2CALL K2_Burst7(gptr k);     /* 0x44D0A0 */
void DC2CALL K2Burst7_Init(gptr k); /* 0x44D0E0 */
void DC2CALL K2Burst7_Looks(gptr k); /* 0x44D1D0 */
void DC2CALL K2_Thing0(gptr k);     /* 0x44CB10 */
void DC2CALL K2Thing0_Init(gptr k); /* 0x44CB50 */
void DC2CALL K2Thing0_Move(gptr k); /* 0x44CCB0 */
void DC2CALL K2_Thing8(gptr k);     /* 0x44D220 */
void DC2CALL K2Thing8_Init(gptr k); /* 0x44D260 */
void DC2CALL K2Thing8_Move(gptr k); /* 0x44D450 */
void DC2CALL K2Thing8_Light(gptr e); /* 0x44D840 */
void DC2CALL K2Thing8_Piece(gptr e, u32 left); /* 0x44D880 */
void DC2CALL Snd_ClearSlots(void);  /* 0x48C040 */
void DC2CALL Strm_HaltAll(void);    /* 0x48C060 */
void DC2CALL Snd_Duck(void);        /* 0x48C8D0 */
void DC2CALL Snd_Unduck(void);      /* 0x48C950 */
void DC2CALL Snd_FadeOutAll(void);  /* 0x48CBA0 */
void DC2CALL Snd_PauseAll(void);    /* 0x48CF60 */
void DC2CALL Snd_ResumeAll(void);   /* 0x48CFB0 */

void DC2CALL Ples12_Remove(gptr o);                /* 0x430E50 */
void DC2CALL Ples12_Type(gptr o);                  /* 0x430360 */
void DC2CALL Ples12_Handler(gptr o);               /* 0x430370 */
void DC2CALL Ples12_Init(gptr o);                  /* 0x430590 */
void DC2CALL Ples12_Normal(gptr o);                /* 0x430820 */
void DC2CALL Ples12_Hit(gptr o);                   /* 0x430850 */
void DC2CALL Ples12_Die(gptr o);                   /* 0x430870 */
void DC2CALL Ples12_Grab(gptr o);                  /* 0x430890 */
u32 DC2CALL Ples12_Decide(gptr o);                 /* 0x430B00 */
void DC2CALL Ples12_ThinkWait(gptr o);             /* 0x4308C0 */
void DC2CALL Ples12_ThinkSwim(gptr o);             /* 0x4309D0 */
void DC2CALL Ples12_Wait2(gptr o);                 /* 0x4308F0 */
void DC2CALL Ples12_Swim(gptr o);                  /* 0x430A30 */
void DC2CALL Ples12_Leap(gptr o);                  /* 0x430C40 */
void DC2CALL Ples12_Dive(gptr o);                  /* 0x430D80 */
void DC2CALL Ples12_Rise(gptr o);                  /* 0x430EB0 */
void DC2CALL Ples12_Lunge(gptr o);                 /* 0x430F80 */
void DC2CALL Ples12_Bite(gptr o);                  /* 0x4310E0 */
void DC2CALL Ples12_Side(gptr o);                  /* 0x431190 */
void DC2CALL Ples12_Resurface(gptr o);             /* 0x4312D0 */
void DC2CALL Ples12_BiteHit(gptr o);               /* 0x4313E0 */
void DC2CALL Ples12_SideHit(gptr o);               /* 0x431440 */
void DC2CALL Ples12_ResurfaceHit(gptr o);          /* 0x4314D0 */
void DC2CALL Ples12_SnapHit(gptr o);               /* 0x431530 */
void DC2CALL Ples12_Damage(gptr o);                /* 0x4315B0 */
void DC2CALL Ples12_Flinch(gptr o);                /* 0x431810 */
void DC2CALL Ples12_Knock(gptr o);                 /* 0x431860 */
void DC2CALL Ples12_Stun(gptr o);                  /* 0x4318C0 */
void DC2CALL Ples12_Dead(gptr o);                  /* 0x431970 */

#endif
