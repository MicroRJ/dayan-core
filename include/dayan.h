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
#define DAY_PATH_CAPACITY 32768

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

typedef struct day_Env_Field
{
	day_String name;
	day_String value;
}
day_Env_Field;

typedef struct day_Env_Table
{
	day_Env_Field *items;
	day_u32 count;
}
day_Env_Table;

typedef struct day_Process
{
	day_uptr handle;
	day_uptr standard_output;
	day_uptr standard_error;
}
day_Process;

typedef struct day_Process_Options
{
	day_String working_directory;
	day_b32 capture_output;
	day_b32 capture_error;
	day_b32 hide_window;
}
day_Process_Options;

typedef enum day_Process_Stream
{
	DAY_PROCESS_OUTPUT,
	DAY_PROCESS_ERROR,
}
day_Process_Stream;

typedef enum day_Standard_Stream
{
	DAY_STANDARD_OUTPUT,
	DAY_STANDARD_ERROR,
}
day_Standard_Stream;

#define DAY_WAIT_INFINITE ((day_u32)-1)

typedef day_u32 day_Thread_Function(void *context);

typedef struct day_Thread
{
	day_uptr handle;
}
day_Thread;

typedef struct day_Mutex
{
	day_u64 storage[8];
}
day_Mutex;

typedef struct day_Condition
{
	day_u64 storage[8];
}
day_Condition;

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

typedef struct day_Path_Builder
{
	char *data;
	day_u64 size;
	day_u64 capacity;
}
day_Path_Builder;

typedef day_u64 day_Path_Mark;

typedef struct day_Directory
{
	day_uptr value;
}
day_Directory;

typedef struct day_Directory_Entry
{
	// The name view remains valid until the next call or the directory is closed.
	day_String name;
	day_File_Info info;
}
day_Directory_Entry;

typedef enum day_Directory_Status
{
	DAY_DIRECTORY_END,
	DAY_DIRECTORY_ENTRY,
}
day_Directory_Status;

#define DAY_LIT(text) ((day_String){ .data = (char *)(text), .size = sizeof(text) - 1 })

void *day_virtual_reserve(day_u64 size);
day_b32 day_virtual_commit(void *memory, day_u64 size);
void day_virtual_release(void *memory, day_u64 size);

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

day_b32 day_path_builder_init(day_Path_Builder *path, char *storage, day_u64 capacity, day_String root);
day_Path_Mark day_path_mark(const day_Path_Builder *path);
day_b32 day_path_push(day_Path_Builder *path, day_String component);
void day_path_pop(day_Path_Builder *path, day_Path_Mark mark);

day_Result day_get_executable_path(day_Arena *arena, day_String *path);
day_Result day_get_current_directory(day_Arena *arena, day_String *path);
day_Result day_set_current_directory(day_String path);
day_Result day_get_absolute_path(day_Arena *arena, day_String path, day_String *absolute);
day_b32 day_executable_resolves(day_String name);
day_Result day_copy_file(day_String source, day_String destination, day_b32 overwrite);
day_Result day_remove_file(day_String path);
day_Result day_move_file(day_String source, day_String destination, day_b32 overwrite);
day_Result day_create_directory(day_String path);
day_Result day_create_directories(day_String path);
day_Result day_remove_directory(day_String path);
day_Result day_remove_tree(day_String path);
day_Result day_find_first_file(day_Path_Builder *path, day_Directory *directory, day_Directory_Entry *entry, day_Directory_Status *status);
day_Result day_find_next_file(day_Directory *directory, day_Directory_Entry *entry, day_Directory_Status *status);
day_Result day_close_directory(day_Directory *directory);

day_Result day_get_env_field(day_Arena *arena, day_String name, day_String *value);
day_Result day_get_env_table(day_Arena *arena, day_Env_Table *table);
day_Result day_set_env_field(day_String name, day_String value);
day_Result day_remove_env_field(day_String name);

day_Result day_start_process(day_String command, day_Process_Options options, day_Process *process);
day_b32 day_process_is_valid(day_Process process);
day_Result day_read_process(day_Process *process, day_Process_Stream stream, void *data, day_u64 capacity, day_u64 *size, day_b32 *end_of_stream);
day_Result day_wait_process(day_Process process, day_u32 milliseconds, day_b32 *completed, day_u32 *exit_code);
day_Result day_close_process(day_Process *process);
day_u64 day_current_process_id(void);
void day_exit_process(day_i32 exit_code);
void day_debug_break(void);

day_Result day_start_thread(day_Thread_Function *function, void *context, day_Thread *thread);
day_b32 day_thread_is_valid(day_Thread thread);
day_Result day_join_thread(day_Thread thread, day_u32 *return_code);
day_Result day_close_thread(day_Thread *thread);
day_u64 day_current_thread_id(void);

day_Result day_init_mutex(day_Mutex *mutex);
void day_lock_mutex(day_Mutex *mutex);
void day_unlock_mutex(day_Mutex *mutex);
void day_destroy_mutex(day_Mutex *mutex);

day_Result day_init_condition(day_Condition *condition);
day_Result day_wait_condition(day_Condition *condition, day_Mutex *mutex);
void day_signal_condition(day_Condition *condition);
void day_broadcast_condition(day_Condition *condition);
void day_destroy_condition(day_Condition *condition);

day_u64 day_counter(void);
day_u64 day_counter_frequency(void);
day_i64 day_unix_time_ms(void);
void day_sleep(day_u64 milliseconds);

day_b32 day_stream_is_console(day_Standard_Stream stream);
day_b32 day_console_supports_colors(day_Standard_Stream stream);
day_Result day_enable_console_colors(day_Standard_Stream stream);
day_Result day_write_console(day_Standard_Stream stream, const void *data, day_u64 size, day_u64 *written);

day_Result day_error_message(day_Arena *arena, day_u32 os_error, day_String *message);

#endif
