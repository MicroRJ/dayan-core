#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <string.h>

static day_Error day_win32_process_error(DWORD error)
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

static char *day_win32_process_text(day_Arena *arena, day_String string, day_b32 convert_separators)
{
	char *result;
	if ((!string.data && string.size) || string.size == UINT64_MAX) return NULL;
	result = day_arena_reserve(arena, string.size + 1);
	if (!result) return NULL;
	for (day_u64 index = 0; index < string.size; ++index)
	{
		char character = string.data[index];
		result[index] = convert_separators && character == '/' ? '\\' : character;
	}
	result[string.size] = 0;
	arena->used += string.size + 1;
	return result;
}

static void day_win32_close_process_handle(HANDLE *handle)
{
	if (*handle) CloseHandle(*handle);
	*handle = NULL;
}

static day_b32 day_win32_create_process_pipe(HANDLE *read_pipe, HANDLE *write_pipe)
{
	SECURITY_ATTRIBUTES security = {0};
	security.nLength = sizeof(security);
	security.bInheritHandle = TRUE;
	if (!CreatePipe(read_pipe, write_pipe, &security, 0)) return 0;
	if (SetHandleInformation(*read_pipe, HANDLE_FLAG_INHERIT, 0)) return 1;
	day_win32_close_process_handle(read_pipe);
	day_win32_close_process_handle(write_pipe);
	return 0;
}

day_Result day_start_process(day_String command, day_Process_Options options, day_Process *process_result)
{
	day_Result result = {0};
	day_Scratch scratch;
	STARTUPINFOA startup = {0};
	PROCESS_INFORMATION process = {0};
	HANDLE output_read = NULL;
	HANDLE output_write = NULL;
	HANDLE error_read = NULL;
	HANDLE error_write = NULL;
	char *mutable_command;
	char *working_directory = NULL;
	day_b32 inherit_handles = options.capture_output || options.capture_error;

	if (process_result) *process_result = (day_Process){0};
	if (!process_result || !command.data || command.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_scratch();
	mutable_command = day_win32_process_text(scratch.arena, command, 0);
	if (options.working_directory.size)
	{
		working_directory = day_win32_process_text(scratch.arena, options.working_directory, 1);
	}
	if (!mutable_command || (options.working_directory.size && !working_directory))
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	if (options.capture_output && !day_win32_create_process_pipe(&output_read, &output_write)) goto failure;
	if (options.capture_error && !day_win32_create_process_pipe(&error_read, &error_write)) goto failure;

	startup.cb = sizeof(startup);
	if (inherit_handles)
	{
		startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		startup.hStdOutput = output_write ? output_write : GetStdHandle(STD_OUTPUT_HANDLE);
		startup.hStdError = error_write ? error_write : GetStdHandle(STD_ERROR_HANDLE);
	}
	if (!CreateProcessA(NULL, mutable_command, NULL, NULL, inherit_handles,
		options.hide_window ? CREATE_NO_WINDOW : 0, NULL, working_directory, &startup, &process)) goto failure;

	day_win32_close_process_handle(&process.hThread);
	day_win32_close_process_handle(&output_write);
	day_win32_close_process_handle(&error_write);
	process_result->handle = (day_uptr)process.hProcess;
	process_result->standard_output = (day_uptr)output_read;
	process_result->standard_error = (day_uptr)error_read;
	day_end_scratch(scratch);
	return result;

failure:
	result.os_error = GetLastError();
	result.error = day_win32_process_error(result.os_error);
	day_win32_close_process_handle(&process.hThread);
	day_win32_close_process_handle(&process.hProcess);
	day_win32_close_process_handle(&output_read);
	day_win32_close_process_handle(&output_write);
	day_win32_close_process_handle(&error_read);
	day_win32_close_process_handle(&error_write);
	day_end_scratch(scratch);
	return result;
}

day_b32 day_process_is_valid(day_Process process)
{
	return process.handle != 0;
}

day_Result day_read_process(day_Process *process, day_Process_Stream stream, void *data,
	day_u64 capacity, day_u64 *size, day_b32 *end_of_stream)
{
	day_Result result = {0};
	day_uptr *pipe_value;
	HANDLE pipe;
	DWORD available = 0;
	DWORD received = 0;

	if (size) *size = 0;
	if (end_of_stream) *end_of_stream = 0;
	if (!process || !size || !end_of_stream || (!data && capacity) ||
		(stream != DAY_PROCESS_OUTPUT && stream != DAY_PROCESS_ERROR))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	pipe_value = stream == DAY_PROCESS_OUTPUT ? &process->standard_output : &process->standard_error;
	pipe = (HANDLE)*pipe_value;
	if (!pipe)
	{
		*end_of_stream = 1;
		return result;
	}
	if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_BROKEN_PIPE)
		{
			CloseHandle(pipe);
			*pipe_value = 0;
			*end_of_stream = 1;
			result.os_error = 0;
			return result;
		}
		result.error = day_win32_process_error(result.os_error);
		return result;
	}
	if (available == 0 || capacity == 0) return result;
	DWORD request = capacity < available ? (DWORD)capacity : available;
	if (capacity > MAXDWORD && available == MAXDWORD) request = MAXDWORD;
	if (!ReadFile(pipe, data, request, &received, NULL))
	{
		result.os_error = GetLastError();
		result.error = day_win32_process_error(result.os_error);
		return result;
	}
	*size = received;
	return result;
}

day_Result day_wait_process(day_Process process, day_u32 milliseconds, day_b32 *completed, day_u32 *exit_code)
{
	day_Result result = {0};
	DWORD wait;
	DWORD code;
	if (completed) *completed = 0;
	if (exit_code) *exit_code = 0;
	if (!completed || !exit_code || !day_process_is_valid(process))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	wait = WaitForSingleObject((HANDLE)process.handle, milliseconds);
	if (wait == WAIT_TIMEOUT) return result;
	if (wait != WAIT_OBJECT_0)
	{
		result.os_error = GetLastError();
		result.error = day_win32_process_error(result.os_error);
		return result;
	}
	if (!GetExitCodeProcess((HANDLE)process.handle, &code))
	{
		result.os_error = GetLastError();
		result.error = day_win32_process_error(result.os_error);
		return result;
	}
	*completed = 1;
	*exit_code = code;
	return result;
}

day_Result day_close_process(day_Process *process)
{
	day_Result result = {0};
	HANDLE handles[3];
	if (!process)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	handles[0] = (HANDLE)process->handle;
	handles[1] = (HANDLE)process->standard_output;
	handles[2] = (HANDLE)process->standard_error;
	for (day_u32 index = 0; index < DAY_ARRAY_COUNT(handles); ++index)
	{
		if (handles[index] && !CloseHandle(handles[index]) && result.error == DAY_ERROR_NONE)
		{
			result.os_error = GetLastError();
			result.error = day_win32_process_error(result.os_error);
		}
	}
	*process = (day_Process){0};
	return result;
}

day_u64 day_current_process_id(void)
{
	return GetCurrentProcessId();
}

void day_exit_process(day_i32 exit_code)
{
	ExitProcess((UINT)exit_code);
}
