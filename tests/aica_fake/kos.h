/* Stand-ins for KallistiOS (tests/aica_test.c): just what aica_sfx.c uses. */
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#define __ARCH_TYPES_H
typedef unsigned long uint32; typedef unsigned long uint8;
unsigned long long timer_us_gettime64(void);
