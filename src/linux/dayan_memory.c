#include "../dayan_memory.h"

#include <stdint.h>
#include <sys/mman.h>

void *day_virtual_reserve(day_u64 size)
{
	void *memory;
	if (size > SIZE_MAX) return NULL;
	memory = mmap(NULL, (size_t)size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return memory == MAP_FAILED ? NULL : memory;
}

day_b32 day_virtual_commit(void *memory, day_u64 size)
{
	if (size > SIZE_MAX) return 0;
	return mprotect(memory, (size_t)size, PROT_READ | PROT_WRITE) == 0;
}

void day_virtual_release(void *memory, day_u64 size)
{
	if (memory && size <= SIZE_MAX) munmap(memory, (size_t)size);
}
