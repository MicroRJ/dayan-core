#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdint.h>
#include <string.h>

static day_Error day_win32_env_error(DWORD error)
{
	switch (error)
	{
		case ERROR_SUCCESS: return DAY_ERROR_NONE;
		case ERROR_INVALID_PARAMETER: return DAY_ERROR_INVALID_ARGUMENT;
		case ERROR_ENVVAR_NOT_FOUND: return DAY_ERROR_NOT_FOUND;
		case ERROR_ACCESS_DENIED: return DAY_ERROR_ACCESS_DENIED;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY: return DAY_ERROR_OUT_OF_MEMORY;
		case ERROR_NOT_SUPPORTED:
		case ERROR_CALL_NOT_IMPLEMENTED: return DAY_ERROR_NOT_SUPPORTED;
		case ERROR_INSUFFICIENT_BUFFER:
		case ERROR_MORE_DATA: return DAY_ERROR_BUFFER_TOO_SMALL;
		default: return DAY_ERROR_UNKNOWN;
	}
}

static char *day_win32_env_text(day_Arena *arena, day_String string)
{
	char *result;
	if ((!string.data && string.size) || string.size == UINT64_MAX) return NULL;
	result = day_arena_reserve(arena, string.size + 1);
	if (!result) return NULL;
	if (string.size) memcpy(result, string.data, (size_t)string.size);
	result[string.size] = 0;
	arena->used += string.size + 1;
	return result;
}

static day_b32 day_win32_env_name_is_valid(day_String name)
{
	if (!name.data || name.size == 0) return 0;
	for (day_u64 index = 0; index < name.size; ++index)
	{
		if (name.data[index] == 0 || name.data[index] == '=') return 0;
	}
	return 1;
}

day_Result day_get_env_field(day_Arena *arena, day_String name, day_String *value)
{
	day_Result result = {0};
	day_Scratch scratch;
	day_u64 mark;
	char *native_name;
	char *data;
	DWORD required;
	DWORD length;

	if (value) *value = (day_String){0};
	if (!arena || !value || !day_win32_env_name_is_valid(name))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_different_scratch(arena);
	native_name = day_win32_env_text(scratch.arena, name);
	if (!native_name)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	SetLastError(ERROR_SUCCESS);
	required = GetEnvironmentVariableA(native_name, NULL, 0);
	if (!required)
	{
		result.os_error = GetLastError();
		if (result.os_error != ERROR_SUCCESS)
		{
			result.error = day_win32_env_error(result.os_error);
			day_end_scratch(scratch);
			return result;
		}
		required = 1;
	}
	mark = day_arena_mark(arena);
	data = day_arena_reserve(arena, required);
	if (!data)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	SetLastError(ERROR_SUCCESS);
	length = GetEnvironmentVariableA(native_name, data, required);
	if (length >= required || (length == 0 && GetLastError() != ERROR_SUCCESS))
	{
		result.os_error = GetLastError();
		result.error = result.os_error ? day_win32_env_error(result.os_error) : DAY_ERROR_BUFFER_TOO_SMALL;
		day_arena_restore(arena, mark);
		day_end_scratch(scratch);
		return result;
	}
	arena->used += (day_u64)length + 1;
	*value = day_string_from_data(data, length);
	day_end_scratch(scratch);
	return result;
}

day_Result day_get_env_table(day_Arena *arena, day_Env_Table *table)
{
	day_Result result = {0};
	day_u64 mark;
	LPCH block;
	LPCH cursor;
	day_u64 count = 0;

	if (table) *table = (day_Env_Table){0};
	if (!arena || !table)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	block = GetEnvironmentStringsA();
	if (!block)
	{
		result.os_error = GetLastError();
		result.error = day_win32_env_error(result.os_error);
		return result;
	}
	for (cursor = block; *cursor; cursor += strlen(cursor) + 1)
	{
		if (count == UINT32_MAX)
		{
			result.error = DAY_ERROR_OUT_OF_MEMORY;
			FreeEnvironmentStringsA(block);
			return result;
		}
		++count;
	}

	mark = day_arena_mark(arena);
	day_arena_align(arena, _Alignof(day_Env_Field));
	table->items = day_arena_push_zero(arena, count * sizeof(*table->items));
	if (!table->items && count)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		FreeEnvironmentStringsA(block);
		day_arena_restore(arena, mark);
		return result;
	}

	for (cursor = block; *cursor; cursor += strlen(cursor) + 1)
	{
		day_u64 size = (day_u64)strlen(cursor);
		day_u64 separator = cursor[0] == '=' ? 1 : 0;
		day_Env_Field *field;
		while (separator < size && cursor[separator] != '=') ++separator;
		if (separator == size) continue;
		field = table->items + table->count;
		field->name.data = day_arena_push_data(arena, cursor, separator);
		field->name.size = separator;
		if (!field->name.data || !day_arena_push_char(arena, 0)) goto out_of_memory;
		field->value.data = day_arena_push_data(arena, cursor + separator + 1, size - separator - 1);
		field->value.size = size - separator - 1;
		if (!field->value.data || !day_arena_push_char(arena, 0)) goto out_of_memory;
		++table->count;
	}
	FreeEnvironmentStringsA(block);
	return result;

out_of_memory:
	result.error = DAY_ERROR_OUT_OF_MEMORY;
	FreeEnvironmentStringsA(block);
	day_arena_restore(arena, mark);
	*table = (day_Env_Table){0};
	return result;
}

day_Result day_set_env_field(day_String name, day_String value)
{
	day_Result result = {0};
	day_Scratch scratch;
	char *native_name;
	char *native_value;
	if (!day_win32_env_name_is_valid(name) || (!value.data && value.size))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_scratch();
	native_name = day_win32_env_text(scratch.arena, name);
	native_value = day_win32_env_text(scratch.arena, value);
	if (!native_name || !native_value)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	if (!SetEnvironmentVariableA(native_name, native_value))
	{
		result.os_error = GetLastError();
		result.error = day_win32_env_error(result.os_error);
	}
	day_end_scratch(scratch);
	return result;
}

day_Result day_remove_env_field(day_String name)
{
	day_Result result = {0};
	day_Scratch scratch;
	char *native_name;
	if (!day_win32_env_name_is_valid(name))
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_scratch();
	native_name = day_win32_env_text(scratch.arena, name);
	if (!native_name)
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	if (!SetEnvironmentVariableA(native_name, NULL))
	{
		result.os_error = GetLastError();
		if (result.os_error == ERROR_ENVVAR_NOT_FOUND) result.os_error = 0;
		result.error = day_win32_env_error(result.os_error);
	}
	day_end_scratch(scratch);
	return result;
}
