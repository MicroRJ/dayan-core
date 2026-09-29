#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "dayan_win32.h"

static HANDLE day_win32_standard_stream(day_Standard_Stream stream)
{
	if (stream == DAY_STANDARD_OUTPUT) return GetStdHandle(STD_OUTPUT_HANDLE);
	if (stream == DAY_STANDARD_ERROR) return GetStdHandle(STD_ERROR_HANDLE);
	return NULL;
}

day_b32 day_stream_is_console(day_Standard_Stream stream)
{
	HANDLE handle = day_win32_standard_stream(stream);
	DWORD mode;
	return handle && handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) != 0;
}

day_b32 day_console_supports_colors(day_Standard_Stream stream)
{
	HANDLE handle = day_win32_standard_stream(stream);
	DWORD mode;
	return handle && handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) &&
		(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

day_Result day_enable_console_colors(day_Standard_Stream stream)
{
	day_Result result = {0};
	HANDLE handle = day_win32_standard_stream(stream);
	DWORD mode;
	if (!handle || handle == INVALID_HANDLE_VALUE)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!GetConsoleMode(handle, &mode))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
		return result;
	}
	if (!SetConsoleMode(handle, mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
	}
	return result;
}

day_Result day_write_console(day_Standard_Stream stream, const void *data, day_u64 size, day_u64 *written)
{
	day_Result result = {0};
	HANDLE handle = day_win32_standard_stream(stream);
	day_u64 total = 0;
	if (written) *written = 0;
	if (!written || !handle || handle == INVALID_HANDLE_VALUE || (!data && size))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	while (total < size)
	{
		day_u64 remaining = size - total;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD count;
		if (!WriteFile(handle, (const char *)data + total, request, &count, NULL))
		{
			*written = total;
			result.os_error = GetLastError();
			result.error = day_win32_error(result.os_error);
			return result;
		}
		if (count == 0) break;
		total += count;
	}
	*written = total;
	return result;
}
