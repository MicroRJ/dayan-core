#include "dayan.h"

#include <assert.h>
#include <string.h>

typedef struct Thread_Test_Context {
	day_Mutex mutex;
	day_Condition condition;
	day_u32 value;
} Thread_Test_Context;

static day_u32 thread_test_main(void *context_pointer)
{
	Thread_Test_Context *context = context_pointer;
	day_lock_mutex(&context->mutex);
	context->value = 42;
	day_signal_condition(&context->condition);
	day_unlock_mutex(&context->mutex);
	return 7;
}

int main(void)
{
	day_Arena arena = day_arena_create(DAY_KILOBYTES(64));
	assert(arena.data);
	void *memory = day_virtual_reserve(DAY_KILOBYTES(4));
	assert(memory && day_virtual_commit(memory, DAY_KILOBYTES(4)));
	memset(memory, 0x5a, DAY_KILOBYTES(4));
	day_virtual_release(memory, DAY_KILOBYTES(4));

	assert(day_counter_frequency() > 0);
	day_u64 before = day_counter();
	day_sleep(1);
	assert(day_counter() >= before);
	assert(day_unix_time_ms() > 0);

	assert(day_remove_tree(DAY_LIT("build/dayan_test")).error == DAY_ERROR_NONE);
	assert(day_create_directories(DAY_LIT("build/dayan_test/a/b")).error == DAY_ERROR_NONE);
	day_File file;
	day_u64 transferred;
	day_u64 size;
	const char expected[] = "dayan";
	char actual[sizeof(expected)] = {0};
	assert(day_access_file(DAY_LIT("build/dayan_test/a/source.tmp"), DAY_FILE_CREATE_ALWAYS,
		DAY_FILE_READ | DAY_FILE_WRITE | DAY_FILE_SHARE_READ, &file).error == DAY_ERROR_NONE);
	assert(day_file_is_valid(file));
	assert(day_write_file(file, expected, sizeof(expected), &transferred).error == DAY_ERROR_NONE);
	assert(transferred == sizeof(expected));
	assert(day_get_file_size(file, &size).error == DAY_ERROR_NONE && size == sizeof(expected));
	day_u64 position;
	assert(day_set_file_cursor(file, DAY_SEEK_BEGIN, 0, &position).error == DAY_ERROR_NONE && position == 0);
	assert(day_read_file(file, actual, sizeof(actual), &transferred).error == DAY_ERROR_NONE);
	assert(transferred == sizeof(actual) && memcmp(actual, expected, sizeof(expected)) == 0);
	assert(day_close_file(file).error == DAY_ERROR_NONE);

	day_File_Info info;
	assert(day_get_file_info(DAY_LIT("build/dayan_test/a/source.tmp"), &info).error == DAY_ERROR_NONE);
	assert(info.size == sizeof(expected) && !info.is_directory && info.modified_unix_ms > 0);
	assert(day_move_file(DAY_LIT("build/dayan_test/a/source.tmp"),
		DAY_LIT("build/dayan_test/a/moved.tmp"), 0).error == DAY_ERROR_NONE);
	assert(day_copy_file(DAY_LIT("build/dayan_test/a/moved.tmp"),
		DAY_LIT("build/dayan_test/a/copied.tmp"), 0).error == DAY_ERROR_NONE);
	assert(day_remove_file(DAY_LIT("build/dayan_test/a/copied.tmp")).error == DAY_ERROR_NONE);

	char path_storage[1024];
	day_Path_Builder path;
	day_Directory directory;
	day_Directory_Entry entry;
	day_Directory_Status directory_status;
	day_b32 found_moved = 0;
	assert(day_path_builder_init(&path, path_storage, sizeof(path_storage), DAY_LIT("build/dayan_test/a")));
	assert(day_find_first_file(&path, &directory, &entry, &directory_status).error == DAY_ERROR_NONE);
	while (directory_status == DAY_DIRECTORY_ENTRY) {
		if (day_string_is(entry.name, "moved.tmp")) found_moved = 1;
		assert(day_find_next_file(&directory, &entry, &directory_status).error == DAY_ERROR_NONE);
	}
	assert(found_moved);
	assert(day_close_directory(&directory).error == DAY_ERROR_NONE);

	day_String current_directory;
	day_String absolute_path;
	day_String executable_path;
	assert(day_get_current_directory(&arena, &current_directory).error == DAY_ERROR_NONE);
	assert(day_set_current_directory(current_directory).error == DAY_ERROR_NONE);
	assert(day_get_absolute_path(&arena, DAY_LIT("."), &absolute_path).error == DAY_ERROR_NONE);
	assert(absolute_path.size > 0);
	assert(day_get_executable_path(&arena, &executable_path).error == DAY_ERROR_NONE);
	assert(executable_path.size > 0);
	assert(day_executable_resolves(DAY_LIT("cmd")));

	assert(day_set_env_field(DAY_LIT("DAYAN_TEST_VALUE"), DAY_LIT("platapuss")).error == DAY_ERROR_NONE);
	day_String environment_value;
	assert(day_get_env_field(&arena, DAY_LIT("DAYAN_TEST_VALUE"), &environment_value).error == DAY_ERROR_NONE);
	assert(day_string_is(environment_value, "platapuss"));
	day_Env_Table environment;
	assert(day_get_env_table(&arena, &environment).error == DAY_ERROR_NONE && environment.count > 0);
	assert(day_remove_env_field(DAY_LIT("DAYAN_TEST_VALUE")).error == DAY_ERROR_NONE);

	day_Process missing_process;
	day_Result missing = day_start_process(DAY_LIT("dayan-test-command-that-does-not-exist"),
		(day_Process_Options){0}, &missing_process);
	assert(missing.error == DAY_ERROR_NOT_FOUND && missing.os_error != 0);
	day_String error_message;
	assert(day_error_message(&arena, missing.os_error, &error_message).error == DAY_ERROR_NONE);
	assert(error_message.size > 0);

	day_Process process;
	assert(day_start_process(DAY_LIT("cmd.exe /d /c echo platapuss"), (day_Process_Options){
		.capture_output = 1,
		.capture_error = 1,
		.hide_window = 1,
	}, &process).error == DAY_ERROR_NONE);
	day_b32 completed;
	day_u32 exit_code;
	assert(day_wait_process(process, UINT32_MAX, &completed, &exit_code).error == DAY_ERROR_NONE);
	assert(completed && exit_code == 0);
	char process_output[64] = {0};
	day_b32 end_of_stream;
	assert(day_read_process(&process, DAY_PROCESS_OUTPUT, process_output, sizeof(process_output),
		&transferred, &end_of_stream).error == DAY_ERROR_NONE);
	assert(transferred >= sizeof("platapuss") - 1);
	assert(memcmp(process_output, "platapuss", sizeof("platapuss") - 1) == 0);
	assert(day_close_process(&process).error == DAY_ERROR_NONE);

	Thread_Test_Context thread_context = {0};
	day_Thread thread;
	day_u32 thread_return_code;
	assert(day_init_mutex(&thread_context.mutex).error == DAY_ERROR_NONE);
	assert(day_init_condition(&thread_context.condition).error == DAY_ERROR_NONE);
	day_lock_mutex(&thread_context.mutex);
	assert(day_start_thread(thread_test_main, &thread_context, &thread).error == DAY_ERROR_NONE);
	while (thread_context.value == 0) assert(day_wait_condition(&thread_context.condition,
		&thread_context.mutex).error == DAY_ERROR_NONE);
	day_unlock_mutex(&thread_context.mutex);
	assert(day_join_thread(thread, &thread_return_code).error == DAY_ERROR_NONE);
	assert(thread_return_code == 7);
	assert(day_close_thread(&thread).error == DAY_ERROR_NONE);
	day_destroy_condition(&thread_context.condition);
	day_destroy_mutex(&thread_context.mutex);

	day_u64 written;
	assert(day_write_console(DAY_STANDARD_OUTPUT, NULL, 0, &written).error == DAY_ERROR_NONE && written == 0);
	(void)day_stream_is_console(DAY_STANDARD_OUTPUT);
	(void)day_console_supports_colors(DAY_STANDARD_OUTPUT);

	assert(day_remove_tree(DAY_LIT("build/dayan_test")).error == DAY_ERROR_NONE);
	day_arena_destroy(&arena);
	return 0;
}
