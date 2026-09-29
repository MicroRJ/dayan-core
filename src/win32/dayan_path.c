#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <string.h>

#include "dayan_win32.h"

static void day_win32_canonicalize_path(char *path, day_u64 size)
{
	for (day_u64 index = 0; index < size; ++index)
	{
		if (path[index] == '\\') path[index] = '/';
	}
}

typedef struct day_Win32_Directory
{
	HANDLE find;
	WIN32_FIND_DATAA data;
}
day_Win32_Directory;

static void day_win32_directory_entry(day_Win32_Directory *directory, day_Directory_Entry *entry)
{
	ULARGE_INTEGER size;
	size.LowPart = directory->data.nFileSizeLow;
	size.HighPart = directory->data.nFileSizeHigh;
	*entry = (day_Directory_Entry){
		.name = day_string_from_cstring(directory->data.cFileName),
		.info = {
			.size = size.QuadPart,
			.created_unix_ms = day_win32_file_time_to_unix_ms(directory->data.ftCreationTime),
			.accessed_unix_ms = day_win32_file_time_to_unix_ms(directory->data.ftLastAccessTime),
			.modified_unix_ms = day_win32_file_time_to_unix_ms(directory->data.ftLastWriteTime),
			.is_directory = (directory->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
			.is_symbolic_link = (directory->data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0,
		},
	};
}

static day_Result day_win32_find_next_file(day_Win32_Directory *directory,
	day_Directory_Entry *entry, day_Directory_Status *status, day_b32 use_current)
{
	day_Result result = {0};
	for (;;)
	{
		if (!use_current && !FindNextFileA(directory->find, &directory->data))
		{
			result.os_error = GetLastError();
			if (result.os_error == ERROR_NO_MORE_FILES)
			{
				result.os_error = 0;
				*status = DAY_DIRECTORY_END;
				return result;
			}
			result.error = day_win32_error(result.os_error);
			return result;
		}
		use_current = 0;
		if (strcmp(directory->data.cFileName, ".") == 0 || strcmp(directory->data.cFileName, "..") == 0) continue;
		day_win32_directory_entry(directory, entry);
		*status = DAY_DIRECTORY_ENTRY;
		return result;
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
		result.error = result.os_error ? day_win32_error(result.os_error) : DAY_ERROR_BUFFER_TOO_SMALL;
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
		result.error = day_win32_error(result.os_error);
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
		result.error = result.os_error ? day_win32_error(result.os_error) : DAY_ERROR_BUFFER_TOO_SMALL;
		day_arena_restore(arena, mark);
		return result;
	}
	day_win32_canonicalize_path(data, length);
	arena->used += (day_u64)length + 1;
	*path = day_string_from_data(data, length);
	return result;
}

day_Result day_set_current_directory(day_String path)
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
	if (!SetCurrentDirectoryA(native_path))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
	}
	day_end_scratch(scratch);
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
		result.error = day_win32_error(result.os_error);
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
		result.error = result.os_error ? day_win32_error(result.os_error) : DAY_ERROR_BUFFER_TOO_SMALL;
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

day_Result day_copy_file(day_String source, day_String destination, day_b32 overwrite)
{
	day_Result result = {0};
	day_Scratch scratch;
	char *native_source;
	char *native_destination;
	if (!source.data || source.size == 0 || !destination.data || destination.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_scratch();
	native_source = day_win32_path_text(scratch.arena, source);
	native_destination = day_win32_path_text(scratch.arena, destination);
	if (!native_source || !native_destination)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	if (!CopyFileA(native_source, native_destination, !overwrite))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
	}
	day_end_scratch(scratch);
	return result;
}

day_Result day_remove_file(day_String path)
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
	if (!DeleteFileA(native_path))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_FILE_NOT_FOUND || result.os_error == ERROR_PATH_NOT_FOUND) result.os_error = 0;
		result.error = day_win32_error(result.os_error);
	}
	day_end_scratch(scratch);
	return result;
}

day_Result day_move_file(day_String source, day_String destination, day_b32 overwrite)
{
	day_Result result = {0};
	day_Scratch scratch;
	char *native_source;
	char *native_destination;
	DWORD flags = overwrite ? MOVEFILE_REPLACE_EXISTING : 0;
	if (!source.data || source.size == 0 || !destination.data || destination.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_scratch();
	native_source = day_win32_path_text(scratch.arena, source);
	native_destination = day_win32_path_text(scratch.arena, destination);
	if (!native_source || !native_destination)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	if (!MoveFileExA(native_source, native_destination, flags))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
	}
	day_end_scratch(scratch);
	return result;
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
		result.error = day_win32_error(result.os_error);
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
		result.error = day_win32_error(result.os_error);
	}
	day_end_scratch(scratch);
	return result;
}

day_Result day_find_first_file(day_Path_Builder *path, day_Directory *directory,
	day_Directory_Entry *entry, day_Directory_Status *status)
{
	day_Result result = {0};
	day_Path_Mark mark;
	day_Win32_Directory *state;

	if (directory) *directory = (day_Directory){0};
	if (entry) *entry = (day_Directory_Entry){0};
	if (status) *status = DAY_DIRECTORY_END;
	if (!path || !path->data || !directory || !entry || !status)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	state = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
	if (!state)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		return result;
	}
	mark = day_path_mark(path);
	if (!day_path_push(path, DAY_LIT("*")))
	{
		HeapFree(GetProcessHeap(), 0, state);
		result.error = DAY_ERROR_BUFFER_TOO_SMALL;
		return result;
	}
	state->find = FindFirstFileA(path->data, &state->data);
	day_path_pop(path, mark);
	if (state->find == INVALID_HANDLE_VALUE)
	{
		result.os_error = GetLastError();
		HeapFree(GetProcessHeap(), 0, state);
		if (result.os_error == ERROR_FILE_NOT_FOUND)
		{
			result.os_error = 0;
			return result;
		}
		result.error = day_win32_error(result.os_error);
		return result;
	}
	directory->value = (day_uptr)state;
	return day_win32_find_next_file(state, entry, status, 1);
}

day_Result day_find_next_file(day_Directory *directory, day_Directory_Entry *entry, day_Directory_Status *status)
{
	day_Result result = {0};
	if (entry) *entry = (day_Directory_Entry){0};
	if (status) *status = DAY_DIRECTORY_END;
	if (!directory || !directory->value || !entry || !status)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	return day_win32_find_next_file((day_Win32_Directory *)directory->value, entry, status, 0);
}

day_Result day_close_directory(day_Directory *directory)
{
	day_Result result = {0};
	day_Win32_Directory *state;
	if (!directory)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!directory->value) return result;
	state = (day_Win32_Directory *)directory->value;
	if (!FindClose(state->find))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
	}
	HeapFree(GetProcessHeap(), 0, state);
	*directory = (day_Directory){0};
	return result;
}
