#include "../dayan_memory.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>

void *day_virtual_reserve(day_u64 size)
{
	if (size > SIZE_MAX) return NULL;
	return VirtualAlloc(NULL, (SIZE_T)size, MEM_RESERVE, PAGE_NOACCESS);
}

day_b32 day_virtual_commit(void *memory, day_u64 size)
{
	if (size > SIZE_MAX) return 0;
	return VirtualAlloc(memory, (SIZE_T)size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

void day_virtual_release(void *memory, day_u64 size)
{
	(void)size;
	if (memory) VirtualFree(memory, 0, MEM_RELEASE);
}
