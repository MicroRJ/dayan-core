#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>

static day_Error day_win32_path_error(DWORD error)
{
	switch (error)
	{
		case ERROR_SUCCESS: return DAY_ERROR_NONE;
		case ERROR_INVALID_PARAMETER: return DAY_ERROR_INVALID_ARGUMENT;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND: return DAY_ERROR_NOT_FOUND;
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

static char *day_win32_path_text(day_Arena *arena, day_String path)
{
	char *result;
	if ((!path.data && path.size) || path.size == UINT64_MAX) return NULL;
	result = day_arena_reserve(arena, path.size + 1);
	if (!result) return NULL;
	for (day_u64 index = 0; index < path.size; ++index)
	{
		result[index] = path.data[index] == '/' ? '\\' : path.data[index];
	}
	result[path.size] = 0;
	arena->used += path.size + 1;
	return result;
}

static void day_win32_canonicalize_path(char *path, day_u64 size)
{
	for (day_u64 index = 0; index < size; ++index)
	{
		if (path[index] == '\\') path[index] = '/';
	}
}

day_Result day_get_executable_path(day_Arena *arena, day_String *path)
{
	day_Result result = {0};
	day_u64 mark;
	char *data;
	DWORD length;
	const day_u64 capacity = 32768;

	if (path) *path = (day_String){0};
	if (!arena || !path)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	mark = day_arena_mark(arena);
	data = day_arena_reserve(arena, capacity);
	if (!data)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		return result;
	}
	length = GetModuleFileNameA(NULL, data, (DWORD)capacity);
	if (!length || length >= capacity)
	{
		result.os_error = GetLastError();
		result.error = result.os_error ? day_win32_path_error(result.os_error) : DAY_ERROR_BUFFER_TOO_SMALL;
		day_arena_restore(arena, mark);
		return result;
	}
	day_win32_canonicalize_path(data, length);
	arena->used += (day_u64)length + 1;
	*path = day_string_from_data(data, length);
	return result;
}

day_Result day_get_current_directory(day_Arena *arena, day_String *path)
{
	day_Result result = {0};
	day_u64 mark;
	DWORD required;
	DWORD length;
	char *data;

	if (path) *path = (day_String){0};
	if (!arena || !path)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	required = GetCurrentDirectoryA(0, NULL);
	if (!required)
	{
		result.os_error = GetLastError();
		result.error = day_win32_path_error(result.os_error);
		return result;
	}
	mark = day_arena_mark(arena);
	data = day_arena_reserve(arena, required);
	if (!data)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		return result;
	}
	length = GetCurrentDirectoryA(required, data);
	if (!length || length >= required)
	{
		result.os_error = GetLastError();
		result.error = result.os_error ? day_win32_path_error(result.os_error) : DAY_ERROR_BUFFER_TOO_SMALL;
		day_arena_restore(arena, mark);
		return result;
	}
	day_win32_canonicalize_path(data, length);
	arena->used += (day_u64)length + 1;
	*path = day_string_from_data(data, length);
	return result;
}

day_Result day_get_absolute_path(day_Arena *arena, day_String path, day_String *absolute_path)
{
	day_Result result = {0};
	day_Scratch scratch;
	day_u64 mark;
	char *native_path;
	char *absolute;
	DWORD required;
	DWORD length;

	if (absolute_path) *absolute_path = (day_String){0};
	if (!arena || !absolute_path || !path.data || path.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_different_scratch(arena);
	native_path = day_win32_path_text(scratch.arena, path);
	if (!native_path)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	required = GetFullPathNameA(native_path, 0, NULL, NULL);
	if (!required)
	{
		result.os_error = GetLastError();
		result.error = day_win32_path_error(result.os_error);
		day_end_scratch(scratch);
		return result;
	}
	mark = day_arena_mark(arena);
	absolute = day_arena_reserve(arena, required);
	if (!absolute)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	length = GetFullPathNameA(native_path, required, absolute, NULL);
	if (!length || length >= required)
	{
		result.os_error = GetLastError();
		result.error = result.os_error ? day_win32_path_error(result.os_error) : DAY_ERROR_BUFFER_TOO_SMALL;
		day_arena_restore(arena, mark);
		day_end_scratch(scratch);
		return result;
	}
	day_win32_canonicalize_path(absolute, length);
	arena->used += (day_u64)length + 1;
	*absolute_path = day_string_from_data(absolute, length);
	day_end_scratch(scratch);
	return result;
}

day_b32 day_executable_resolves(day_String name)
{
	day_Scratch scratch;
	char *native_name;
	char *resolved;
	DWORD length;
	const day_u64 capacity = 32768;

	if (!name.data || name.size == 0) return 0;
	scratch = day_begin_scratch();
	native_name = day_win32_path_text(scratch.arena, name);
	resolved = day_arena_push(scratch.arena, capacity);
	if (!native_name || !resolved)
	{
		day_end_scratch(scratch);
		return 0;
	}
	length = SearchPathA(NULL, native_name, ".exe", (DWORD)capacity, resolved, NULL);
	day_end_scratch(scratch);
	return length > 0 && length < capacity;
}

day_Result day_create_directory(day_String path)
{
	day_Result result = {0};
	day_Scratch scratch;
	char *native_path;
	DWORD attributes;

	if (!path.data || path.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_scratch();
	native_path = day_win32_path_text(scratch.arena, path);
	if (!native_path)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	if (!CreateDirectoryA(native_path, NULL))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_ALREADY_EXISTS)
		{
			attributes = GetFileAttributesA(native_path);
			if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) result.os_error = 0;
		}
		result.error = day_win32_path_error(result.os_error);
	}
	day_end_scratch(scratch);
	return result;
}

day_Result day_remove_directory(day_String path)
{
	day_Result result = {0};
	day_Scratch scratch;
	char *native_path;

	if (!path.data || path.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_scratch();
	native_path = day_win32_path_text(scratch.arena, path);
	if (!native_path)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	if (!RemoveDirectoryA(native_path))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_FILE_NOT_FOUND || result.os_error == ERROR_PATH_NOT_FOUND) result.os_error = 0;
		result.error = day_win32_path_error(result.os_error);
	}
	day_end_scratch(scratch);
	return result;
}
