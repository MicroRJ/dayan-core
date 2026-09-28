#ifndef DAYAN_MEMORY_H
#define DAYAN_MEMORY_H

#include "dayan.h"

void *day_virtual_reserve(day_u64 size);
day_b32 day_virtual_commit(void *memory, day_u64 size);
void day_virtual_release(void *memory, day_u64 size);

#endif
