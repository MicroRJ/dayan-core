#include "dayan.h"

#if defined(_WIN32)
#include "win32/dayan_process.c"
#else
#error unsupported operating system
#endif
