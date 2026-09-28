#ifndef DAYAN_WIN32_H
#define DAYAN_WIN32_H

static inline day_Error day_win32_error(DWORD error)
{
	switch (error)
	{
		case ERROR_SUCCESS: return DAY_ERROR_NONE;
		case ERROR_INVALID_PARAMETER: return DAY_ERROR_INVALID_ARGUMENT;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
		case ERROR_ENVVAR_NOT_FOUND: return DAY_ERROR_NOT_FOUND;
		case ERROR_ACCESS_DENIED: return DAY_ERROR_ACCESS_DENIED;
		case ERROR_ALREADY_EXISTS:
		case ERROR_FILE_EXISTS: return DAY_ERROR_ALREADY_EXISTS;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY: return DAY_ERROR_OUT_OF_MEMORY;
		case ERROR_NOT_SUPPORTED:
		case ERROR_CALL_NOT_IMPLEMENTED: return DAY_ERROR_NOT_SUPPORTED;
		case ERROR_INSUFFICIENT_BUFFER:
		case ERROR_MORE_DATA: return DAY_ERROR_BUFFER_TOO_SMALL;
		default: return DAY_ERROR_UNKNOWN;
	}
}

static inline char *day_win32_text(day_Arena *arena, day_String string)
{
	char *result;
	if (!arena || (!string.data && string.size) || string.size == UINT64_MAX) return NULL;
	result = day_arena_reserve(arena, string.size + 1);
	if (!result) return NULL;
	if (string.size) memcpy(result, string.data, (size_t)string.size);
	result[string.size] = 0;
	arena->used += string.size + 1;
	return result;
}

static inline char *day_win32_path_text(day_Arena *arena, day_String path)
{
	char *result = day_win32_text(arena, path);
	if (!result) return NULL;
	for (day_u64 index = 0; index < path.size; ++index)
	{
		if (result[index] == '/') result[index] = '\\';
	}
	return result;
}

#endif
