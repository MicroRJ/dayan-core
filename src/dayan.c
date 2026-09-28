#include "dayan.h"
#include "dayan_memory.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)
#define DAY_THREAD_LOCAL __declspec(thread)
#else
#define DAY_THREAD_LOCAL _Thread_local
#endif

#define DAY_SCRATCH_ARENA_COUNT 4
#define DAY_COMMIT_GRANULARITY DAY_KILOBYTES(64)

static DAY_THREAD_LOCAL day_Arena day_scratch_arenas[DAY_SCRATCH_ARENA_COUNT];

day_Arena day_arena_create(day_u64 initial_reserve)
{
	day_Arena arena = {0};
	if (initial_reserve == 0) initial_reserve = DAY_MEGABYTES(64);
	arena.data = day_virtual_reserve(initial_reserve);
	if (arena.data) arena.capacity = initial_reserve;
	return arena;
}

void day_arena_set_name(day_Arena *arena, const char *name)
{
	if (arena) arena->name = name;
}

void day_arena_destroy(day_Arena *arena)
{
	if (!arena) return;
	day_virtual_release(arena->data, arena->capacity);
	memset(arena, 0, sizeof(*arena));
}

void day_arena_reset(day_Arena *arena)
{
	if (arena) arena->used = 0;
}

day_u64 day_arena_mark(day_Arena *arena)
{
	return arena ? arena->used : 0;
}

void day_arena_restore(day_Arena *arena, day_u64 mark)
{
	assert(arena && mark <= arena->used);
	if (arena && mark <= arena->used) arena->used = mark;
}

void *day_arena_top(day_Arena *arena)
{
	return arena && arena->data ? arena->data + arena->used : NULL;
}

void *day_arena_reserve(day_Arena *arena, day_u64 size)
{
	day_u64 needed;
	day_u64 committed;

	if (!arena || !arena->data || size > arena->capacity - arena->used)
	{
		assert(!"arena capacity exceeded");
		return NULL;
	}

	needed = arena->used + size;
	if (needed > arena->committed)
	{
		committed = (needed + DAY_COMMIT_GRANULARITY - 1) & ~(DAY_COMMIT_GRANULARITY - 1);
		if (committed > arena->capacity) committed = arena->capacity;
		if (!day_virtual_commit(arena->data + arena->committed, committed - arena->committed))
		{
			assert(!"unable to commit arena memory");
			return NULL;
		}
		arena->committed = committed;
	}

	return arena->data + arena->used;
}

void day_arena_align(day_Arena *arena, day_u64 alignment)
{
	uintptr_t top;
	day_u64 padding;
	assert(arena && alignment && (alignment & (alignment - 1)) == 0);
	if (!arena || !alignment || (alignment & (alignment - 1)) != 0) return;
	top = (uintptr_t)day_arena_top(arena);
	padding = (alignment - top % alignment) % alignment;
	day_arena_push(arena, padding);
}

void *day_arena_push(day_Arena *arena, day_u64 size)
{
	void *result = day_arena_reserve(arena, size);
	if (result) arena->used += size;
	return result;
}

void *day_arena_push_zero(day_Arena *arena, day_u64 size)
{
	void *result = day_arena_push(arena, size);
	if (result && size) memset(result, 0, (size_t)size);
	return result;
}

void *day_arena_push_copy(day_Arena *arena, day_u64 size, const void *data)
{
	void *result = day_arena_push(arena, size);
	if (result && size) memcpy(result, data, (size_t)size);
	return result;
}

char *day_arena_push_data(day_Arena *arena, const void *data, day_u64 size)
{
	return day_arena_push_copy(arena, size, data);
}

char *day_arena_push_text(day_Arena *arena, const char *text)
{
	return day_arena_push_data(arena, text, text ? (day_u64)strlen(text) : 0);
}

char *day_arena_push_char(day_Arena *arena, char character)
{
	char *result = day_arena_push(arena, 1);
	if (result) *result = character;
	return result;
}

void day_arena_push_nchar(day_Arena *arena, char character, day_u64 count)
{
	void *result = day_arena_push(arena, count);
	if (result && count) memset(result, character, (size_t)count);
}

char *day_arena_pushfv(day_Arena *arena, const char *format, va_list arguments)
{
	va_list copy;
	int length;
	char *result;

	va_copy(copy, arguments);
	length = vsnprintf(NULL, 0, format, copy);
	va_end(copy);
	assert(length >= 0);
	if (length < 0) return NULL;

	result = day_arena_reserve(arena, (day_u64)length + 1);
	if (!result) return NULL;
	if (vsnprintf(result, (size_t)length + 1, format, arguments) != length) return NULL;
	arena->used += (day_u64)length;
	return result;
}

char *day_arena_pushf(day_Arena *arena, const char *format, ...)
{
	va_list arguments;
	char *result;
	va_start(arguments, format);
	result = day_arena_pushfv(arena, format, arguments);
	va_end(arguments);
	return result;
}

day_Scratch day_begin_scratch(void)
{
	return day_begin_different_scratch(NULL);
}

day_Scratch day_begin_different_scratch(day_Arena *conflict)
{
	for (day_u32 index = 0; index < DAY_SCRATCH_ARENA_COUNT; ++index)
	{
		day_Arena *arena = day_scratch_arenas + index;
		if (arena == conflict) continue;
		if (!arena->data)
		{
			*arena = day_arena_create(0);
			day_arena_set_name(arena, "thread scratch");
		}
		assert(arena->data);
		return (day_Scratch){ arena, arena->used };
	}
	assert(!"no non-conflicting scratch arena available");
	return (day_Scratch){0};
}

void day_end_scratch(day_Scratch scratch)
{
	day_arena_restore(scratch.arena, scratch.restore_used);
}

void day_destroy_thread_scratch(void)
{
	for (day_u32 index = 0; index < DAY_SCRATCH_ARENA_COUNT; ++index)
	{
		day_arena_destroy(day_scratch_arenas + index);
	}
}

day_String day_string_from_data(void *data, day_u64 size)
{
	return (day_String){ .data = data, .size = size };
}

day_String day_string_from_range(void *start, void *end)
{
	assert((day_u8 *)end >= (day_u8 *)start);
	if ((day_u8 *)end < (day_u8 *)start) return (day_String){0};
	return day_string_from_data(start, (day_u64)((day_u8 *)end - (day_u8 *)start));
}

day_String day_string_from_cstring(const char *text)
{
	return day_string_from_data((char *)text, text ? (day_u64)strlen(text) : 0);
}

day_String day_string_slice(day_String string, day_u64 offset, day_u64 size)
{
	assert(offset <= string.size && size <= string.size - offset);
	if (offset > string.size || size > string.size - offset) return (day_String){0};
	return day_string_from_data(string.data + offset, size);
}

day_b32 day_string_equal(day_String left, day_String right)
{
	return left.size == right.size && (left.size == 0 || memcmp(left.data, right.data, (size_t)left.size) == 0);
}

static day_u32 day_ascii_lower(day_u32 character)
{
	return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
}

day_b32 day_string_equal_insensitive(day_String left, day_String right)
{
	if (left.size != right.size) return 0;
	for (day_u64 index = 0; index < left.size; ++index)
	{
		if (day_ascii_lower((day_u8)left.data[index]) != day_ascii_lower((day_u8)right.data[index])) return 0;
	}
	return 1;
}

day_b32 day_string_starts_with(day_String string, day_String prefix)
{
	return prefix.size <= string.size && memcmp(string.data, prefix.data, (size_t)prefix.size) == 0;
}

day_b32 day_string_ends_with(day_String string, day_String suffix)
{
	return suffix.size <= string.size && memcmp(string.data + string.size - suffix.size, suffix.data, (size_t)suffix.size) == 0;
}

day_b32 day_string_ends_with_insensitive(day_String string, day_String suffix)
{
	return suffix.size <= string.size && day_string_equal_insensitive(day_string_slice(string, string.size - suffix.size, suffix.size), suffix);
}

day_b32 day_string_is(day_String string, const char *text)
{
	return day_string_equal(string, day_string_from_cstring(text));
}

static day_b32 day_ascii_is_whitespace(char character)
{
	return character == ' ' || character == '\t' || character == '\r' || character == '\n' || character == '\v' || character == '\f';
}

day_String day_string_trim_whitespace(day_String string)
{
	day_u64 start = 0;
	day_u64 end = string.size;
	while (start < end && day_ascii_is_whitespace(string.data[start])) ++start;
	while (end > start && day_ascii_is_whitespace(string.data[end - 1])) --end;
	return day_string_slice(string, start, end - start);
}

day_b32 day_string_split_first(day_String string, char separator, day_String *left, day_String *right)
{
	if (!left || !right) return 0;
	for (day_u64 index = 0; index < string.size; ++index)
	{
		if (string.data[index] == separator)
		{
			*left = day_string_slice(string, 0, index);
			*right = day_string_slice(string, index + 1, string.size - index - 1);
			return 1;
		}
	}
	*left = (day_String){0};
	*right = (day_String){0};
	return 0;
}

day_String_Array day_string_split(day_Arena *arena, day_String string, char separator)
{
	day_String_Array result = {0};
	day_u32 count = 1;
	day_u64 start = 0;

	if (!arena) return result;
	for (day_u64 index = 0; index < string.size; ++index)
	{
		if (string.data[index] == separator)
		{
			if (count == UINT32_MAX) return result;
			++count;
		}
	}

	day_arena_align(arena, _Alignof(day_String));
	result.items = day_arena_push_zero(arena, sizeof(*result.items) * count);
	if (!result.items) return (day_String_Array){0};

	for (day_u64 index = 0; index <= string.size; ++index)
	{
		if (index == string.size || string.data[index] == separator)
		{
			result.items[result.count++] = day_string_slice(string, start, index - start);
			start = index + 1;
		}
	}
	return result;
}

day_String_Array day_string_split_lines(day_Arena *arena, day_String string)
{
	day_String_Array result = day_string_split(arena, string, '\n');
	for (day_u32 index = 0; index < result.count; ++index)
	{
		day_String *line = result.items + index;
		if (line->size && line->data[line->size - 1] == '\r') --line->size;
	}
	return result;
}

day_String_Array day_string_split_block(day_Arena *arena, day_String string)
{
	day_String_Array result = day_string_split(arena, string, 0);
	while (result.count && result.items[result.count - 1].size == 0) --result.count;
	return result;
}

day_u32 day_string_count_lines(day_String string)
{
	day_u32 count = string.size ? 1 : 0;
	for (day_u64 index = 0; index < string.size; ++index)
	{
		if (string.data[index] == '\n') ++count;
	}
	return count;
}
