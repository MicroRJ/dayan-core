#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "dayan_win32.h"

day_u64 day_counter(void)
{
	LARGE_INTEGER value;
	return QueryPerformanceCounter(&value) ? (day_u64)value.QuadPart : 0;
}

day_u64 day_counter_frequency(void)
{
	LARGE_INTEGER value;
	return QueryPerformanceFrequency(&value) ? (day_u64)value.QuadPart : 0;
}

day_i64 day_unix_time_ms(void)
{
	FILETIME time;
	GetSystemTimeAsFileTime(&time);
	return day_win32_file_time_to_unix_ms(time);
}

void day_sleep(day_u64 milliseconds)
{
	while (milliseconds >= MAXDWORD)
	{
		Sleep(MAXDWORD - 1);
		milliseconds -= MAXDWORD - 1;
	}
	Sleep((DWORD)milliseconds);
}
