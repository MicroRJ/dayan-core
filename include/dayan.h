#ifndef DAYAN_H
#define DAYAN_H

#include <stdarg.h>
#include <stdint.h>

typedef uint8_t  day_u8;
typedef uint16_t day_u16;
typedef uint32_t day_u32;
typedef uint64_t day_u64;
typedef int8_t   day_i8;
typedef int16_t  day_i16;
typedef int32_t  day_i32;
typedef int64_t  day_i64;
typedef float    day_f32;
typedef double   day_f64;
typedef day_i32  day_b32;
typedef uintptr_t day_uptr;

#define DAY_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define DAY_KILOBYTES(value) ((day_u64)(value) << 10)
#define DAY_MEGABYTES(value) ((day_u64)(value) << 20)
#define DAY_GIGABYTES(value) ((day_u64)(value) << 30)

typedef struct day_Arena
{
	day_u64 capacity;
	day_u64 committed;
	day_u64 used;
	day_u8 *data;
	const char *name;
}
day_Arena;

typedef struct day_Scratch
{
	day_Arena *arena;
	day_u64 restore_used;
}
day_Scratch;

typedef struct day_String
{
	union
	{
		char *data;
		char *text;
	};
	day_u64 size;
}
day_String;

typedef struct day_String_Array
{
	day_String *items;
	day_u32 count;
}
day_String_Array;

typedef enum day_Error
{
	DAY_ERROR_NONE,
	DAY_ERROR_UNKNOWN,
	DAY_ERROR_INVALID_ARGUMENT,
	DAY_ERROR_NOT_FOUND,
	DAY_ERROR_ACCESS_DENIED,
	DAY_ERROR_ALREADY_EXISTS,
	DAY_ERROR_OUT_OF_MEMORY,
	DAY_ERROR_NOT_SUPPORTED,
	DAY_ERROR_BUFFER_TOO_SMALL,
}
day_Error;

typedef struct day_Result
{
	day_Error error;
	day_u32 os_error;
}
day_Result;

typedef struct day_File
{
	day_uptr value;
}
day_File;

typedef enum day_File_Access
{
	DAY_FILE_READ         = 1 << 0,
	DAY_FILE_WRITE        = 1 << 1,
	DAY_FILE_EXECUTE      = 1 << 2,
	DAY_FILE_SHARE_READ   = 1 << 3,
	DAY_FILE_SHARE_WRITE  = 1 << 4,
	DAY_FILE_SHARE_DELETE = 1 << 5,
	DAY_FILE_NO_BUFFERING = 1 << 6,
}
day_File_Access;

typedef enum day_File_Intent
{
	DAY_FILE_CREATE_ALWAYS,
	DAY_FILE_CREATE_NEW,
	DAY_FILE_OPEN_ALWAYS,
	DAY_FILE_OPEN_EXISTING,
	DAY_FILE_TRUNCATE_EXISTING,
}
day_File_Intent;

typedef enum day_Seek_Origin
{
	DAY_SEEK_BEGIN,
	DAY_SEEK_CURRENT,
	DAY_SEEK_END,
}
day_Seek_Origin;

typedef struct day_File_Info
{
	day_u64 size;
	day_i64 created_unix_ms;
	day_i64 accessed_unix_ms;
	day_i64 modified_unix_ms;
	day_b32 is_directory;
	day_b32 is_symbolic_link;
}
day_File_Info;

#define DAY_LIT(text) ((day_String){ .data = (char *)(text), .size = sizeof(text) - 1 })

day_Arena day_arena_create(day_u64 initial_reserve);
void day_arena_set_name(day_Arena *arena, const char *name);
void day_arena_destroy(day_Arena *arena);
void day_arena_reset(day_Arena *arena);
day_u64 day_arena_mark(day_Arena *arena);
void day_arena_restore(day_Arena *arena, day_u64 mark);
void *day_arena_top(day_Arena *arena);
void *day_arena_reserve(day_Arena *arena, day_u64 size);
void day_arena_align(day_Arena *arena, day_u64 alignment);
void *day_arena_push(day_Arena *arena, day_u64 size);
void *day_arena_push_zero(day_Arena *arena, day_u64 size);
void *day_arena_push_copy(day_Arena *arena, day_u64 size, const void *data);
char *day_arena_push_data(day_Arena *arena, const void *data, day_u64 size);
char *day_arena_push_text(day_Arena *arena, const char *text);
char *day_arena_push_char(day_Arena *arena, char character);
void day_arena_push_nchar(day_Arena *arena, char character, day_u64 count);
char *day_arena_pushfv(day_Arena *arena, const char *format, va_list arguments);
char *day_arena_pushf(day_Arena *arena, const char *format, ...);

day_Scratch day_begin_scratch(void);
day_Scratch day_begin_different_scratch(day_Arena *conflict);
void day_end_scratch(day_Scratch scratch);
void day_destroy_thread_scratch(void);

day_String day_string_from_data(void *data, day_u64 size);
day_String day_string_from_range(void *start, void *end);
day_String day_string_from_cstring(const char *text);
day_String day_string_slice(day_String string, day_u64 offset, day_u64 size);
day_b32 day_string_equal(day_String left, day_String right);
day_b32 day_string_equal_insensitive(day_String left, day_String right);
day_b32 day_string_starts_with(day_String string, day_String prefix);
day_b32 day_string_ends_with(day_String string, day_String suffix);
day_b32 day_string_ends_with_insensitive(day_String string, day_String suffix);
day_b32 day_string_is(day_String string, const char *text);
day_String day_string_trim_whitespace(day_String string);
day_b32 day_string_split_first(day_String string, char separator, day_String *left, day_String *right);
day_String_Array day_string_split(day_Arena *arena, day_String string, char separator);
day_String_Array day_string_split_lines(day_Arena *arena, day_String string);
day_String_Array day_string_split_block(day_Arena *arena, day_String string);
day_u32 day_string_count_lines(day_String string);

day_Result day_access_file(day_String path, day_File_Intent intent, day_File_Access access, day_File *file);
day_b32 day_file_is_valid(day_File file);
day_Result day_close_file(day_File file);
day_Result day_get_file_info(day_String path, day_File_Info *info);
day_Result day_get_file_size(day_File file, day_u64 *size);
day_Result day_set_file_cursor(day_File file, day_Seek_Origin origin, day_i64 distance, day_u64 *position);
day_Result day_read_file(day_File file, void *data, day_u64 size, day_u64 *bytes_read);
day_Result day_write_file(day_File file, const void *data, day_u64 size, day_u64 *bytes_written);

day_Result day_get_executable_path(day_Arena *arena, day_String *path);
day_Result day_get_current_directory(day_Arena *arena, day_String *path);
day_Result day_get_absolute_path(day_Arena *arena, day_String path, day_String *absolute);
day_Result day_create_directory(day_String path);
day_Result day_remove_directory(day_String path);

#endif
