#ifndef DC2_HOOKS_H
#define DC2_HOOKS_H
#include "dc2/types.h"
typedef struct { u32 addr; void *fn; const char *name; } Hook;
extern const Hook g_hooks[];
extern const int g_nhooks;
#endif
