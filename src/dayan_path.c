#include "dayan.h"

static day_b32 day_path_is_separator(char character)
{
	return character == '/' || character == '\\';
}

day_b32 day_path_builder_init(day_Path_Builder *path, char *storage, day_u64 capacity, day_String root)
{
	if (path) *path = (day_Path_Builder){0};
	if (!path || !storage || capacity == 0 || (!root.data && root.size) || root.size >= capacity) return 0;
	for (day_u64 index = 0; index < root.size; ++index)
	{
		storage[index] = root.data[index] == '\\' ? '/' : root.data[index];
	}
	storage[root.size] = 0;
	*path = (day_Path_Builder){ .data = storage, .size = root.size, .capacity = capacity };
	return 1;
}

day_Path_Mark day_path_mark(const day_Path_Builder *path)
{
	return path ? path->size : 0;
}

day_b32 day_path_push(day_Path_Builder *path, day_String component)
{
	day_b32 separator;
	if (!path || !path->data || path->capacity == 0 || path->size >= path->capacity || (!component.data && component.size)) return 0;
	separator = path->size && component.size && !day_path_is_separator(path->data[path->size - 1]) && !day_path_is_separator(component.data[0]);
	if (component.size > path->capacity - path->size - 1) return 0;
	if (separator && component.size == path->capacity - path->size - 1) return 0;
	if (separator) path->data[path->size++] = '/';
	for (day_u64 index = 0; index < component.size; ++index)
	{
		path->data[path->size++] = component.data[index] == '\\' ? '/' : component.data[index];
	}
	path->data[path->size] = 0;
	return 1;
}

void day_path_pop(day_Path_Builder *path, day_Path_Mark mark)
{
	if (!path || !path->data || mark > path->size) return;
	path->size = mark;
	path->data[mark] = 0;
}

static day_u64 day_path_root_size(day_String path)
{
	day_u64 root = 0;
	if (path.size >= 2 && path.data[1] == ':') return 2;
	if (path.size && day_path_is_separator(path.data[0])) root = 1;
	if (path.size >= 2 && day_path_is_separator(path.data[0]) && day_path_is_separator(path.data[1]))
	{
		root = 2;
		for (day_u32 component = 0; component < 2 && root < path.size; ++component)
		{
			while (root < path.size && !day_path_is_separator(path.data[root])) ++root;
			while (root < path.size && day_path_is_separator(path.data[root])) ++root;
		}
	}
	return root;
}

day_Result day_create_directories(day_String path)
{
	day_Result result = {0};
	day_Scratch scratch;
	day_Path_Builder builder;
	day_u64 root;
	char *storage;
	if (!path.data || path.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	scratch = day_begin_scratch();
	storage = day_arena_push(scratch.arena, path.size + 1);
	if (!storage || !day_path_builder_init(&builder, storage, path.size + 1, path))
	{
		result.error = DAY_ERROR_OUT_OF_MEMORY;
		day_end_scratch(scratch);
		return result;
	}
	root = day_path_root_size(day_string_from_data(builder.data, builder.size));
	if (root >= builder.size)
	{
		day_File_Info info;
		result = day_get_file_info(day_string_from_data(builder.data, builder.size), &info);
		if (!result.error && !info.is_directory) result.error = DAY_ERROR_ALREADY_EXISTS;
		day_end_scratch(scratch);
		return result;
	}
	for (day_u64 index = root; index < builder.size; ++index)
	{
		if (!day_path_is_separator(builder.data[index]) || index == root) continue;
		builder.data[index] = 0;
		result = day_create_directory(day_string_from_data(builder.data, index));
		builder.data[index] = '/';
		if (result.error)
		{
			day_end_scratch(scratch);
			return result;
		}
	}
	result = day_create_directory(day_string_from_data(builder.data, builder.size));
	day_end_scratch(scratch);
	return result;
}

static day_Result day_remove_tree_builder(day_Path_Builder *path)
{
	day_Result result = {0};
	day_File_Info root;
	day_Directory directory = {0};
	day_Directory_Entry entry;
	day_Directory_Status status;
	day_String root_path = day_string_from_data(path->data, path->size);
	result = day_get_file_info(root_path, &root);
	if (result.error == DAY_ERROR_NOT_FOUND) return (day_Result){0};
	if (result.error) return result;
	if (!root.is_directory)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (root.is_symbolic_link) return day_remove_directory(root_path);
	result = day_find_first_file(path, &directory, &entry, &status);
	while (!result.error && status == DAY_DIRECTORY_ENTRY)
	{
		day_Path_Mark mark = day_path_mark(path);
		if (!day_path_push(path, entry.name)) result.error = DAY_ERROR_BUFFER_TOO_SMALL;
		else if (entry.info.is_directory)
		{
			result = entry.info.is_symbolic_link ?
				day_remove_directory(day_string_from_data(path->data, path->size)) : day_remove_tree_builder(path);
		}
		else result = day_remove_file(day_string_from_data(path->data, path->size));
		day_path_pop(path, mark);
		if (!result.error) result = day_find_next_file(&directory, &entry, &status);
	}
	{
		day_Result close = day_close_directory(&directory);
		if (!result.error) result = close;
	}
	if (!result.error) result = day_remove_directory(root_path);
	return result;
}

day_Result day_remove_tree(day_String path)
{
	day_Result result = {0};
	day_Scratch scratch;
	day_Path_Builder builder;
	char *storage;
	if (!path.data || path.size == 0)
	{
		result.error = DAY_ERROR_INVALID_ARGUMENT;
		return result;
	}
	if (path.size >= DAY_PATH_CAPACITY)
	{
		result.error = DAY_ERROR_BUFFER_TOO_SMALL;
		return result;
	}
	scratch = day_begin_scratch();
	storage = day_arena_push(scratch.arena, DAY_PATH_CAPACITY);
	if (!storage || !day_path_builder_init(&builder, storage, DAY_PATH_CAPACITY, path)) result.error = DAY_ERROR_OUT_OF_MEMORY;
	else result = day_remove_tree_builder(&builder);
	day_end_scratch(scratch);
	return result;
}

#if defined(_WIN32)
#include "win32/dayan_path.c"
#else
#error unsupported operating system
#endif
