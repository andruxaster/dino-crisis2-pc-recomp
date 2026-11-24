/* The camera backgrounds made ahead on the Dreamcast (bgvq.c). */
#ifndef DC2_BGVQ_H
#define DC2_BGVQ_H
#include "dc2/types.h"
void BgVq_Init(void);      /* the hooks set (after the PC layer and the renderer) */
u32 BgVq_Loaded(void);     /* pictures taken from Data\BGVQ (the log) */
u32 BgVq_Missing(void);    /* camera pictures not there: decoded from the JPEG */
#endif
