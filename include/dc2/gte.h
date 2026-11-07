/* PsyQ libgte as reimplemented by the PC port (0x4C08E0..0x4C24C0), rewritten
 * to give the same results bit for bit. Names follow libgte. The emulated
 * GTE keeps its registers in .data (0x87B370..0x87B710, see below); screen
 * coordinates are floats on the PC (SXY FIFO), the rest is integer.
 * Float code follows the x87 sequence of the original (extended precision,
 * rounded to float where the original stores a float). */
#ifndef DC2_GTE_H
#define DC2_GTE_H
#include "types.h"

/* emulated GTE registers */
#define GTE_LCM     0x87B380u   /* light colour matrix (SetColorMatrix), 32 bytes */
#define GTE_BK      0x87B3A8u   /* back colour s16[3] (SetBackColor << 4) */
#define GTE_RGB0    0x87B374u   /* colour FIFO RGB0..2 (u32 each) */
#define GTE_RGB1    0x87B378u
#define GTE_RGB2    0x87B37Cu
#define GTE_IR0     0x87B3B4u   /* depth cue result */
#define GTE_V1      0x87B3B8u   /* SVECTOR */
#define GTE_V0      0x87B3C0u
#define GTE_V2      0x87B3C8u
#define GTE_MAC1    0x87B3D8u   /* s32 x3 */
#define GTE_IR1     0x87B3E8u   /* s16 x3 */
#define GTE_ZNEAR   0x87B3F0u   /* H / 2: nearest z that is projected as is */
#define GTE_RT      0x87B3F8u   /* rotation, 9 x s16 + pad */
#define GTE_TR      0x87B40Cu   /* translation s32 x3 */
#define GTE_FLAG    0x87B418u
#define GTE_DQNEAR  0x87B420u   /* depth cue range */
#define GTE_DQFAR   0x87B370u
#define GTE_SXY     0x87B428u   /* float SX0,SY0,SX1,SY1,SX2,SY2 */
#define GTE_ZSCALE  0x87B440u   /* float, z divisor for Direct3D depth */
#define GTE_LLM     0x87B448u   /* light matrix (SetLightMatrix), 32 bytes */
#define GTE_H       0x87B6E8u
#define GTE_RGBC    0x87B6ECu
#define GTE_MAC0    0x87B6F0u
#define GTE_OFY     0x87B6F4u
#define GTE_OFX     0x87B6F8u
#define GTE_SZ_A    0x87B700u   /* SZ FIFO as the PC code keeps it */
#define GTE_SZ_B    0x87B704u
#define GTE_SZ_C    0x87B708u
#define GTE_SZ3     0x87B70Cu
#define GTE_SINTAB  0x73C064u   /* s16[0x401], quarter wave, 4096 = 1.0 */
#define GTE_IDENT   0x73C050u   /* identity MATRIX */

#define F32(a) (*(float *)MEM(a))

s32  DC2CALL rsin(s32 a);                                            /* 0x4C08E0 */
s32  DC2CALL rcos(s32 a);                                            /* 0x4C0920 */
s32  DC2CALL ratan2(float y, float x);                               /* 0x4C0940 */
s32  DC2CALL SquareRoot0(s32 x);                                     /* 0x4C0960 */
s32  DC2CALL SquareRoot12(s32 x);                                    /* 0x4C0970 */
void DC2CALL InitGeom(void);                                         /* 0x4C0990 */
void DC2CALL SetGeomOffset(s32 ofx, s32 ofy);                        /* 0x4C09E0 */
s32  DC2CALL ReadGeomScreen(void);                                   /* 0x4C0A00 */
void DC2CALL SetGeomScreen(s32 h);                                   /* 0x4C0A10 */
void DC2CALL SetBackColor(s32 r, s32 g, s32 b);                      /* 0x4C0A70 */
gptr DC2CALL ApplyMatrix(gptr m, gptr sv, gptr v);                   /* 0x4C0AA0 */
gptr DC2CALL ApplyMatrixSV(gptr m, gptr sv, gptr out);               /* 0x4C0B20 */
gptr DC2CALL ApplyMatrixLV(gptr m, gptr v, gptr out);                /* 0x4C0BA0 */
gptr DC2CALL ApplyRotMatrixLV(gptr v, gptr out);                     /* 0x4C0C20 */
gptr DC2CALL MulMatrix0(gptr m0, gptr m1, gptr m2);                  /* 0x4C0CB0 */
gptr DC2CALL RotMatrixX(s32 r, gptr m);                              /* 0x4C0E50 */
gptr DC2CALL RotMatrixY(s32 r, gptr m);                              /* 0x4C0EC0 */
gptr DC2CALL RotMatrixZ(s32 r, gptr m);                              /* 0x4C0F30 */
gptr DC2CALL RotMatrix(gptr r, gptr m);                              /* 0x4C0FA0 */
gptr DC2CALL RotMatrixYXZ(gptr r, gptr m);                           /* 0x4C0FF0 */
gptr DC2CALL RotMatrixZXY(gptr r, gptr m);                           /* 0x4C1040 */
gptr DC2CALL RotMatrixZYX(gptr r, gptr m);                           /* 0x4C1090 */
gptr DC2CALL ScaleMatrix(gptr m, gptr v);                            /* 0x4C10E0 */
gptr DC2CALL TransposeMatrix(gptr m, gptr out);                      /* 0x4C1170 */
void DC2CALL OuterProduct12(gptr a, gptr b, gptr out);               /* 0x4C11C0 */
s32  DC2CALL RotTransPers(gptr v, gptr sxy, gptr p, gptr flag);      /* 0x4C1220 */
s32  DC2CALL Gte_DepthCue(s32 sz);                                   /* 0x4C1310 */
void DC2CALL Gte_Project(gptr sx, gptr sy, gptr sz);                 /* 0x4C1350 */
s32  DC2CALL RotTransPers3(gptr v0, gptr v1, gptr v2, gptr sxy0, gptr sxy1, gptr sxy2,
                           gptr p, gptr flag);                       /* 0x4C1470 */
s32  DC2CALL RotTransPers4(gptr v0, gptr v1, gptr v2, gptr v3, gptr sxy0, gptr sxy1,
                           gptr sxy2, gptr sxy3, gptr p, gptr flag); /* 0x4C15D0 */
void DC2CALL VectorNormalS(gptr in, gptr out);                       /* 0x4C1790 */
void DC2CALL Gte_NormalColor(gptr v, gptr rgb, gptr out);            /* 0x4C18E0 */
void DC2CALL Gte_ClampIR(gptr v, gptr sv);                           /* 0x4C19B0 */
void DC2CALL SetLightMatrix(gptr m);                                 /* 0x4C1A30 */
void DC2CALL SetColorMatrix(gptr m);                                 /* 0x4C1A50 */
void DC2CALL LoadAverageShort0(gptr v0, gptr v1, s32 p0, s32 p1, gptr out); /* 0x4C1A70 */
void DC2CALL LoadAverageCol(gptr v0, gptr v1, s32 p0, s32 p1, gptr out);    /* 0x4C1AD0 */
void DC2CALL SetRotMatrix(gptr m);                                   /* 0x4C1B40 */
void DC2CALL SetTransMatrix(gptr m);                                 /* 0x4C1B60 */
void DC2CALL SetDepthScale(float f);                                 /* 0x4C1B90 */
void DC2CALL gte_ldclmv(gptr m);                                     /* 0x4C1BA0 */
void DC2CALL gte_ldrgb(gptr c);                                      /* 0x4C1BD0 */
void DC2CALL gte_ldsxy3(gptr a, gptr b, gptr c);                     /* 0x4C1BE0 */
void DC2CALL gte_ldv0(gptr v);                                       /* 0x4C1C20 */
void DC2CALL gte_ldv3(gptr v0, gptr v1, gptr v2);                    /* 0x4C1C40 */
void DC2CALL gte_ldv3c(gptr v);                                      /* 0x4C1C80 */
void DC2CALL gte_nccs(void);                                         /* 0x4C1CC0 */
void DC2CALL gte_ncct(void);                                         /* 0x4C1CE0 */
void DC2CALL gte_nclip(void);                                        /* 0x4C1D20 */
void DC2CALL gte_rtv0(void);                                         /* 0x4C1D70 */
void DC2CALL gte_rtv1(void);                                         /* 0x4C1DB0 */
void DC2CALL gte_rtv2(void);                                         /* 0x4C1DF0 */
void DC2CALL gte_rtir(void);                                         /* 0x4C1E30 */
void DC2CALL gte_rtps(void);                                         /* 0x4C1E70 */
void DC2CALL gte_rtpt(void);                                         /* 0x4C1F40 */
void DC2CALL gte_stclmv(gptr m);                                     /* 0x4C2090 */
void DC2CALL gte_stlvnl(gptr v);                                     /* 0x4C20C0 */
void DC2CALL gte_stopz(gptr p);                                      /* 0x4C20E0 */
void DC2CALL gte_stflg(gptr p);                                      /* 0x4C20F0 */
void DC2CALL gte_strgb(gptr p);                                      /* 0x4C2100 */
void DC2CALL gte_strgb3(gptr p);                                     /* 0x4C2110 */
void DC2CALL gte_strgb3_gt3(gptr p);                                 /* 0x4C2130, 0x4C2150 */
void DC2CALL gte_stsv(gptr p);                                       /* 0x4C2170 */
void DC2CALL gte_stsxy(gptr p);                                      /* 0x4C2190 */
void DC2CALL gte_stsxy3c(gptr p);                                    /* 0x4C21B0 */
void DC2CALL gte_stsxy3_g3(gptr p);                                  /* 0x4C21F0 */
void DC2CALL gte_stsxy3_gt3(gptr p);                                 /* 0x4C2230, 0x4C2270 */
void DC2CALL gte_stsz(gptr p);                                       /* 0x4C22B0 */
void DC2CALL gte_stsz3(gptr p);                                      /* 0x4C22C0 */
void DC2CALL gte_stsz4(gptr p);                                      /* 0x4C22E0 */
void DC2CALL PC_stsz4_g4(gptr p);                                    /* 0x4C2310, 0x4C23D0 */
void DC2CALL PC_stsz_g4_flat(gptr p);                                /* 0x4C2360 */
void DC2CALL PC_stsz3_g3(gptr p);                                    /* 0x4C2390 */
void DC2CALL PC_stsz3_gt3(gptr p);                                   /* 0x4C2420 */
void DC2CALL PC_stsz4_gt4(gptr p);                                   /* 0x4C2460 */
void DC2CALL gte_stszotz(gptr p);                                    /* 0x4C24B0 */
#endif
