#include "dayan.h"

#if defined(_WIN32)
#include "win32/dayan_thread.c"
#else
#error unsupported operating system
#endif
