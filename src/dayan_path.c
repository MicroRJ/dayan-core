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

#if defined(_WIN32)
#include "win32/dayan_path.c"
#else
#error unsupported operating system
#endif
