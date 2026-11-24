#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "nmmgr.h"
#define O_MODE_MASK 0x0f
#define O_DIR 0x1000
typedef struct { int size; char name[256]; unsigned time; unsigned attr; } dirent_t;
typedef struct vfs_handler {
    nmmgr_handler_t nmmgr; int cache; void *privdata;
    void *(*open)(struct vfs_handler *, const char *, int);
    int (*close)(void *); ssize_t (*read)(void *, void *, size_t);
    off_t (*seek)(void *, off_t, int); off_t (*tell)(void *); size_t (*total)(void *);
    const dirent_t *(*readdir)(void *);
    int (*stat)(struct vfs_handler *, const char *, struct stat *, int);
    int (*fcntl)(void *, int, va_list); int (*rewinddir)(void *); int (*fstat)(void *, struct stat *);
} vfs_handler_t;
