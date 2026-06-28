//
// Created by MI on 25-9-13.
//

#ifndef YSTRING_H
#define YSTRING_H

#include <stddef.h>

// ==================== 字符串结构定义 ====================

typedef struct {
    char* data;        // 字符串数据
    size_t length;     // 字符串长度
    size_t capacity;   // 分配的内存容量
} String;

typedef struct {
    String** items;    // 字符串指针数组
    size_t count;      // 当前字符串数量
    size_t capacity;   // 数组容量
} StringArray;

// ==================== 字符串函数声明 ====================

// 创建和销毁
String* string_create(const char* cstr);
String* string_create_empty();
String* string_create_from_int(int value);
String* string_create_from_double(double value, int precision);
String* string_clone(const String* str);
void string_destroy(String* str);

// 容量管理
int string_ensure_capacity(String* str, size_t min_capacity);
void string_shrink_to_fit(String* str);

// 属性获取
const char* string_cstr(const String* str);
size_t string_length(const String* str);
int string_is_empty(const String* str);
int string_equals(const String* str1, const String* str2);
int string_equals_cstr(const String* str, const char* cstr);
int string_equals_ignore_case(const String* str1, const String* str2);
int string_compare(const String* str1, const String* str2);
int string_starts_with(const String* str, const String* prefix);
int string_ends_with(const String* str, const String* suffix);
char string_char_at(const String* str, size_t index);
int string_char_at_safe(const String* str, size_t index, char* result);

// 类型转换函数
int string_to_int(const String* str, int* result);
int string_to_double(const String* str, double* result);
int string_to_float(const String* str, float* result);
int string_to_bool(const String* str, int* result);
char string_to_char(const String* str);

// 文件操作
int string_write_to_file(const String* str, const char* filename);
String* string_read_from_file(const char* filename);
int string_append_to_file(const String* str, const char* filename);

// 修改操作
void string_append(String* str, const String* other);
void string_append_cstr(String* str, const char* cstr);
void string_append_char(String* str, char c);
void string_append_int(String* str, int value);
void string_append_double(String* str, double value, int precision);
void string_insert(String* str, size_t index, const String* other);
void string_insert_cstr(String* str, size_t index, const char* cstr);
void string_remove(String* str, size_t index, size_t count);
void string_clear(String* str);
void string_set(String* str, const char* cstr);
void string_set_string(String* dest, const String* src);

// 转换操作
void string_to_upper(String* str);
void string_to_lower(String* str);
void string_replace(String* str, const String* old_str, const String* new_str);
void string_replace_cstr(String* str, const char* old_cstr, const char* new_cstr);
void string_trim(String* str);
void string_trim_left(String* str);
void string_trim_right(String* str);

// 子字符串操作
String* string_substring(const String* str, size_t start, size_t end);
int string_index_of(const String* str, char c, size_t start);
int string_index_of_str(const String* str, const String* substr, size_t start);
int string_last_index_of(const String* str, char c, size_t start);
StringArray* string_split(const String* str, const String* delimiter);

// 格式化
String* string_format(const char* format, ...);

// 输出
void string_print(const String* str);

// ==================== 字符串数组函数声明 ====================

StringArray* string_array_create();
StringArray* string_array_create_with_capacity(size_t capacity);
void string_array_destroy(StringArray* array);
void string_array_add(StringArray* array, const String* str);
void string_array_add_cstr(StringArray* array, const char* cstr);
void string_array_insert(StringArray* array, size_t index, const String* str);
void string_array_remove(StringArray* array, size_t index);
void string_array_clear(StringArray* array);
String* string_array_get(const StringArray* array, size_t index);
size_t string_array_size(const StringArray* array);
int string_array_is_empty(const StringArray* array);
void string_array_print(const StringArray* array);
String* string_array_join(const StringArray* array, const String* separator);

// 字符串数组文件操作
int string_array_write_to_file(const StringArray* array, const char* filename, const char* delimiter);
StringArray* string_array_read_from_file(const char* filename, const char* delimiter);

// 控制台输入
String* string_read_line();
String* string_read_line_with_prompt(const char* prompt);
int string_read_line_into(String* str);

#endif //YSTRING_H