#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string.h>

#include "dayan_win32.h"

day_Result day_error_message(day_Arena *arena, day_u32 os_error, day_String *message_result)
{
	day_Result result = {0};
	day_u64 mark;
	char *system_message = NULL;
	char *message;
	DWORD length;
	if (message_result) *message_result = (day_String){0};
	if (!arena || !message_result || os_error == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS, NULL, os_error, 0, (char *)&system_message, 0, NULL);
	if (!length)
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
		return result;
	}
	while (length && (system_message[length - 1] == '\r' || system_message[length - 1] == '\n' ||
		system_message[length - 1] == ' ')) --length;
	mark = day_arena_mark(arena);
	message = day_arena_push_data(arena, system_message, length);
	if (!message || !day_arena_push_char(arena, 0))
	{
		day_arena_restore(arena, mark);
		result.error = DAY_ERROR_OUT_OF_MEMORY;
	}
	else *message_result = day_string_from_data(message, length);
	LocalFree(system_message);
	return result;
}
