#pragma once
typedef int mutex_t;
#define RECURSIVE_MUTEX_INITIALIZER 0
#define mutex_lock(m) ((void)0)
#define mutex_unlock(m) ((void)0)
