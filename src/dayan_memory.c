#include "dayan_memory.h"

#if defined(_WIN32)
#include "win32/dayan_memory.c"
#elif defined(__linux__)
#include "linux/dayan_memory.c"
#else
#error unsupported operating system
#endif
