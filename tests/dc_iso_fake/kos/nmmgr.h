#pragma once
#include <stdint.h>
typedef struct nmmgr_handler { char pathname[256]; int pid; uint32_t version, flags, type; void *list_ent; } nmmgr_handler_t;
#define NMMGR_TYPE_VFS 1
#define NMMGR_LIST_INIT NULL
static inline int nmmgr_handler_add(nmmgr_handler_t *h) { (void)h; return 0; }
