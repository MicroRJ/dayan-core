#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>

static HANDLE day_win32_handle_from_file(day_File file)
{
	return (HANDLE)file.value;
}

static day_Error day_win32_file_error(DWORD error)
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
	default: return DAY_ERROR_UNKNOWN;
	}
}

static day_i64 day_win32_file_time_to_unix_ms(FILETIME time)
{
	ULARGE_INTEGER value;
	const day_u64 unix_epoch = 116444736000000000ull;
	value.LowPart = time.dwLowDateTime;
	value.HighPart = time.dwHighDateTime;
	if (value.QuadPart < unix_epoch) return 0;
	return (day_i64)((value.QuadPart - unix_epoch) / 10000ull);
}

static char *day_win32_file_path(day_Arena *arena, day_String path)
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

day_File_Result day_access_file(day_String path, day_File_Intent intent, day_File_Access access)
{
	day_File_Result result = {0};
	day_Scratch scratch;
	char *native_path;
	DWORD desired_access = 0;
	DWORD share_mode = 0;
	DWORD disposition;
	DWORD attributes = FILE_ATTRIBUTE_NORMAL;
	HANDLE handle;

	if (!path.data || path.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}

	if (access & DAY_FILE_READ) desired_access |= GENERIC_READ;
	if (access & DAY_FILE_WRITE) desired_access |= GENERIC_WRITE;
	if (access & DAY_FILE_EXECUTE) desired_access |= GENERIC_EXECUTE;
	if (access & DAY_FILE_SHARE_READ) share_mode |= FILE_SHARE_READ;
	if (access & DAY_FILE_SHARE_WRITE) share_mode |= FILE_SHARE_WRITE;
	if (access & DAY_FILE_SHARE_DELETE) share_mode |= FILE_SHARE_DELETE;
	if (access & DAY_FILE_NO_BUFFERING) attributes |= FILE_FLAG_NO_BUFFERING;

	switch (intent)
	{
	case DAY_FILE_CREATE_ALWAYS: disposition = CREATE_ALWAYS; break;
	case DAY_FILE_CREATE_NEW: disposition = CREATE_NEW; break;
	case DAY_FILE_OPEN_ALWAYS: disposition = OPEN_ALWAYS; break;
	case DAY_FILE_OPEN_EXISTING: disposition = OPEN_EXISTING; break;
	case DAY_FILE_TRUNCATE_EXISTING: disposition = TRUNCATE_EXISTING; break;
	default:
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}

	scratch = day_begin_scratch();
	native_path = day_win32_file_path(scratch.arena, path);
	if (!native_path)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}

	handle = CreateFileA(native_path, desired_access, share_mode, NULL, disposition, attributes, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		result.os_error = GetLastError();
		result.error = day_win32_file_error(result.os_error);
	}
	else
	{
		result.file.value = (day_uptr)handle;
	}
	day_end_scratch(scratch);
	return result;
}

day_b32 day_file_is_valid(day_File file)
{
	return file.value != 0 && day_win32_handle_from_file(file) != INVALID_HANDLE_VALUE;
}

day_Result day_close_file(day_File file)
{
	day_Result result = {0};
	if (!day_file_is_valid(file))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!CloseHandle(day_win32_handle_from_file(file)))
	{
		result.os_error = GetLastError();
		result.error = day_win32_file_error(result.os_error);
	}
	return result;
}

day_File_Info_Result day_get_file_info(day_String path)
{
	day_File_Info_Result result = {0};
	day_Scratch scratch;
	char *native_path;
	WIN32_FILE_ATTRIBUTE_DATA data;
	ULARGE_INTEGER size;

	if (!path.data || path.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}

	scratch = day_begin_scratch();
	native_path = day_win32_file_path(scratch.arena, path);
	if (!native_path)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	if (!GetFileAttributesExA(native_path, GetFileExInfoStandard, &data))
	{
		result.os_error = GetLastError();
		result.error = day_win32_file_error(result.os_error);
		day_end_scratch(scratch);
		return result;
	}
	day_end_scratch(scratch);

	size.LowPart = data.nFileSizeLow;
	size.HighPart = data.nFileSizeHigh;
	result.info.size = size.QuadPart;
	result.info.created_unix_ms = day_win32_file_time_to_unix_ms(data.ftCreationTime);
	result.info.accessed_unix_ms = day_win32_file_time_to_unix_ms(data.ftLastAccessTime);
	result.info.modified_unix_ms = day_win32_file_time_to_unix_ms(data.ftLastWriteTime);
	result.info.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	result.info.is_symbolic_link = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
	return result;
}

day_File_Size_Result day_get_file_size(day_File file)
{
	day_File_Size_Result result = {0};
	LARGE_INTEGER size;
	if (!day_file_is_valid(file))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!GetFileSizeEx(day_win32_handle_from_file(file), &size) || size.QuadPart < 0)
	{
		result.os_error = GetLastError();
		result.error = day_win32_file_error(result.os_error);
		return result;
	}
	result.size = (day_u64)size.QuadPart;
	return result;
}

day_File_Seek_Result day_set_file_cursor(day_File file, day_Seek_Origin origin, day_i64 distance)
{
	day_File_Seek_Result result = {0};
	static const DWORD origins[] = { FILE_BEGIN, FILE_CURRENT, FILE_END };
	LARGE_INTEGER move;
	LARGE_INTEGER position;
	if (!day_file_is_valid(file) || origin < DAY_SEEK_BEGIN || origin > DAY_SEEK_END)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	move.QuadPart = distance;
	if (!SetFilePointerEx(day_win32_handle_from_file(file), move, &position, origins[origin]) || position.QuadPart < 0)
	{
		result.os_error = GetLastError();
		result.error = day_win32_file_error(result.os_error);
		return result;
	}
	result.position = (day_u64)position.QuadPart;
	return result;
}

day_IO_Result day_read_file(day_File file, void *data, day_u64 size)
{
	day_IO_Result result = {0};
	if (!day_file_is_valid(file) || (!data && size))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	while (result.size < size)
	{
		day_u64 remaining = size - result.size;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD received = 0;
		if (!ReadFile(day_win32_handle_from_file(file), (char *)data + result.size, request, &received, NULL))
		{
			result.os_error = GetLastError();
			result.error = day_win32_file_error(result.os_error);
			return result;
		}
		result.size += received;
		if (received < request) break;
	}
	return result;
}

day_IO_Result day_write_file(day_File file, const void *data, day_u64 size)
{
	day_IO_Result result = {0};
	if (!day_file_is_valid(file) || (!data && size))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	while (result.size < size)
	{
		day_u64 remaining = size - result.size;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD written = 0;
		if (!WriteFile(day_win32_handle_from_file(file), (const char *)data + result.size, request, &written, NULL))
		{
			result.os_error = GetLastError();
			result.error = day_win32_file_error(result.os_error);
			return result;
		}
		result.size += written;
		if (written == 0)
		{
			result.error = DAY_ERROR_UNKNOWN;
			return result;
		}
	}
	return result;
}
