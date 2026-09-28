#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <string.h>

#include "dayan_win32.h"

static HANDLE day_win32_handle_from_file(day_File file)
{
	return (HANDLE)file.value;
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

day_Result day_access_file(day_String path, day_File_Intent intent, day_File_Access access, day_File *file)
{
	day_Result result = {0};
	day_Scratch scratch;
	char *native_path;
	DWORD desired_access = 0;
	DWORD share_mode = 0;
	DWORD disposition;
	DWORD attributes = FILE_ATTRIBUTE_NORMAL;
	HANDLE handle;

	if (file) *file = (day_File){0};
	if (!file || !path.data || path.size == 0)
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
		default: result.error = DAY_ERROR_INVALID_ARGUMENT; return result;
	}

	scratch = day_begin_scratch();
	native_path = day_win32_path_text(scratch.arena, path);
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
		result.error = day_win32_error(result.os_error);
	}
	else
	{
		file->value = (day_uptr)handle;
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
		result.error = day_win32_error(result.os_error);
	}
	return result;
}

day_Result day_get_file_info(day_String path, day_File_Info *info)
{
	day_Result result = {0};
	day_Scratch scratch;
	char *native_path;
	WIN32_FILE_ATTRIBUTE_DATA data;
	ULARGE_INTEGER size;

	if (info) *info = (day_File_Info){0};
	if (!info || !path.data || path.size == 0)
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
	if (!GetFileAttributesExA(native_path, GetFileExInfoStandard, &data))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
		day_end_scratch(scratch);
		return result;
	}
	day_end_scratch(scratch);

	size.LowPart = data.nFileSizeLow;
	size.HighPart = data.nFileSizeHigh;
	info->size = size.QuadPart;
	info->created_unix_ms = day_win32_file_time_to_unix_ms(data.ftCreationTime);
	info->accessed_unix_ms = day_win32_file_time_to_unix_ms(data.ftLastAccessTime);
	info->modified_unix_ms = day_win32_file_time_to_unix_ms(data.ftLastWriteTime);
	info->is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	info->is_symbolic_link = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
	return result;
}

day_Result day_get_file_size(day_File file, day_u64 *size)
{
	day_Result result = {0};
	LARGE_INTEGER value;
	if (size) *size = 0;
	if (!size || !day_file_is_valid(file))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!GetFileSizeEx(day_win32_handle_from_file(file), &value) || value.QuadPart < 0)
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
		return result;
	}
	*size = (day_u64)value.QuadPart;
	return result;
}

day_Result day_set_file_cursor(day_File file, day_Seek_Origin origin, day_i64 distance, day_u64 *position)
{
	day_Result result = {0};
	static const DWORD origins[] = { FILE_BEGIN, FILE_CURRENT, FILE_END };
	LARGE_INTEGER move;
	LARGE_INTEGER value;
	if (position) *position = 0;
	if (!position || !day_file_is_valid(file) || origin < DAY_SEEK_BEGIN || origin > DAY_SEEK_END)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	move.QuadPart = distance;
	if (!SetFilePointerEx(day_win32_handle_from_file(file), move, &value, origins[origin]) || value.QuadPart < 0)
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
		return result;
	}
	*position = (day_u64)value.QuadPart;
	return result;
}

day_Result day_read_file(day_File file, void *data, day_u64 size, day_u64 *bytes_read)
{
	day_Result result = {0};
	day_u64 total = 0;
	if (bytes_read) *bytes_read = 0;
	if (!bytes_read || !day_file_is_valid(file) || (!data && size))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	while (total < size)
	{
		day_u64 remaining = size - total;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD received = 0;
		if (!ReadFile(day_win32_handle_from_file(file), (char *)data + total, request, &received, NULL))
		{
			*bytes_read = total;
			result.os_error = GetLastError();
			result.error = day_win32_error(result.os_error);
			return result;
		}
		total += received;
		if (received < request) break;
	}
	*bytes_read = total;
	return result;
}

day_Result day_write_file(day_File file, const void *data, day_u64 size, day_u64 *bytes_written)
{
	day_Result result = {0};
	day_u64 total = 0;
	if (bytes_written) *bytes_written = 0;
	if (!bytes_written || !day_file_is_valid(file) || (!data && size))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	while (total < size)
	{
		day_u64 remaining = size - total;
		DWORD request = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
		DWORD written = 0;
		if (!WriteFile(day_win32_handle_from_file(file), (const char *)data + total, request, &written, NULL))
		{
			*bytes_written = total;
			result.os_error = GetLastError();
			result.error = day_win32_error(result.os_error);
			return result;
		}
		total += written;
		if (written == 0)
		{
			*bytes_written = total;
			result.error = DAY_ERROR_UNKNOWN;
			return result;
		}
	}
	*bytes_written = total;
	return result;
}
