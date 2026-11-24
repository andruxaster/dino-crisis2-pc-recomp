/* Files streamed as they play (music, speech) read ahead into the AICA's
 * sound RAM instead of main RAM (aica_ra.c). */
#ifndef DC2_AICA_RA_H
#define DC2_AICA_RA_H
#include <stddef.h>
#include "dc2/types.h"
/* a FILE reading path with `ring` bytes of read-ahead in sound RAM; NULL
   when there is no room there or the file does not open */
void *AicaRa_Open(const char *path, size_t ring);
u32 AicaRa_Open_Count(void);    /* such files open (the log) */
u32 AicaRa_Pieces(void);        /* pieces read from the disc (the log) */
#endif
