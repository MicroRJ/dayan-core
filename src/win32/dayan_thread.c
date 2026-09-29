#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "dayan_win32.h"

typedef struct day_Win32_Thread_Start
{
	day_Thread_Function *function;
	void *context;
}
day_Win32_Thread_Start;

_Static_assert(sizeof(day_Mutex) >= sizeof(SRWLOCK), "day_Mutex storage is too small");
_Static_assert(_Alignof(day_Mutex) >= _Alignof(SRWLOCK), "day_Mutex storage is under-aligned");
_Static_assert(sizeof(day_Condition) >= sizeof(CONDITION_VARIABLE), "day_Condition storage is too small");
_Static_assert(_Alignof(day_Condition) >= _Alignof(CONDITION_VARIABLE), "day_Condition storage is under-aligned");

static DWORD WINAPI day_win32_thread_entry(void *context)
{
	day_Win32_Thread_Start start = *(day_Win32_Thread_Start *)context;
	HeapFree(GetProcessHeap(), 0, context);
	return start.function(start.context);
}

day_Result day_start_thread(day_Thread_Function *function, void *context, day_Thread *thread_result)
{
	day_Result result = {0};
	day_Win32_Thread_Start *start;
	HANDLE thread;
	if (thread_result) *thread_result = (day_Thread){0};
	if (!function || !thread_result)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	start = HeapAlloc(GetProcessHeap(), 0, sizeof(*start));
	if (!start)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		result.os_error = ERROR_NOT_ENOUGH_MEMORY;
		return result;
	}
	start->function = function;
	start->context = context;
	thread = CreateThread(NULL, 0, day_win32_thread_entry, start, 0, NULL);
	if (!thread)
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
		HeapFree(GetProcessHeap(), 0, start);
		return result;
	}
	thread_result->handle = (day_uptr)thread;
	return result;
}

day_b32 day_thread_is_valid(day_Thread thread)
{
	return thread.handle != 0;
}

day_Result day_join_thread(day_Thread thread, day_u32 *return_code)
{
	day_Result result = {0};
	DWORD code;
	if (return_code) *return_code = 0;
	if (!return_code || !day_thread_is_valid(thread))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (WaitForSingleObject((HANDLE)thread.handle, INFINITE) != WAIT_OBJECT_0)
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
		return result;
	}
	if (!GetExitCodeThread((HANDLE)thread.handle, &code))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
		return result;
	}
	*return_code = code;
	return result;
}

day_Result day_close_thread(day_Thread *thread)
{
	day_Result result = {0};
	if (!thread)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (thread->handle && !CloseHandle((HANDLE)thread->handle))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
	}
	*thread = (day_Thread){0};
	return result;
}

day_u64 day_current_thread_id(void)
{
	return GetCurrentThreadId();
}

day_Result day_init_mutex(day_Mutex *mutex)
{
	day_Result result = {0};
	if (!mutex)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	*mutex = (day_Mutex){0};
	InitializeSRWLock((SRWLOCK *)mutex->storage);
	return result;
}

void day_lock_mutex(day_Mutex *mutex)
{
	if (mutex) AcquireSRWLockExclusive((SRWLOCK *)mutex->storage);
}

void day_unlock_mutex(day_Mutex *mutex)
{
	if (mutex) ReleaseSRWLockExclusive((SRWLOCK *)mutex->storage);
}

void day_destroy_mutex(day_Mutex *mutex)
{
	if (mutex) *mutex = (day_Mutex){0};
}

day_Result day_init_condition(day_Condition *condition)
{
	day_Result result = {0};
	if (!condition)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	*condition = (day_Condition){0};
	InitializeConditionVariable((CONDITION_VARIABLE *)condition->storage);
	return result;
}

day_Result day_wait_condition(day_Condition *condition, day_Mutex *mutex)
{
	day_Result result = {0};
	if (!condition || !mutex)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (!SleepConditionVariableSRW((CONDITION_VARIABLE *)condition->storage,
		(SRWLOCK *)mutex->storage, INFINITE, 0))
	{
		result.os_error = GetLastError();
		result.error = day_win32_error(result.os_error);
	}
	return result;
}

void day_signal_condition(day_Condition *condition)
{
	if (condition) WakeConditionVariable((CONDITION_VARIABLE *)condition->storage);
}

void day_broadcast_condition(day_Condition *condition)
{
	if (condition) WakeAllConditionVariable((CONDITION_VARIABLE *)condition->storage);
}

void day_destroy_condition(day_Condition *condition)
{
	if (condition) *condition = (day_Condition){0};
}
