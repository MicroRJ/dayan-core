#include "dayan.h"

#if defined(_WIN32)
#include "win32/dayan_console.c"
#else
#error unsupported operating system
#endif
