#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include "YString.h"

String* string_create(const char* cstr) {
    if (!cstr) return NULL;

    String* str = (String*)malloc(sizeof(String));
    if (!str) return NULL;

    str->length = strlen(cstr);
    str->capacity = str->length + 1;
    str->data = (char*)malloc(str->capacity);

    if (!str->data) {
        free(str);
        return NULL;
    }

    strcpy(str->data, cstr);
    return str;
}

String* string_create_empty() {
    String* str = (String*)malloc(sizeof(String));
    if (!str) return NULL;

    str->length = 0;
    str->capacity = 16;
    str->data = (char*)malloc(str->capacity);

    if (!str->data) {
        free(str);
        return NULL;
    }

    str->data[0] = '\0';
    return str;
}

String* string_create_from_int(int value) {
    String* str = string_create_empty();
    if (!str) return NULL;

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    string_set(str, buffer);

    return str;
}

String* string_create_from_double(double value, int precision) {
    String* str = string_create_empty();
    if (!str) return NULL;

    char format[16];
    snprintf(format, sizeof(format), "%%.%df", precision);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), format, value);
    string_set(str, buffer);

    return str;
}

String* string_clone(const String* str) {
    if (!str) return NULL;
    return string_create(str->data);
}

void string_destroy(String* str) {
    if (str) {
        free(str->data);
        free(str);
    }
}

int string_ensure_capacity(String* str, size_t min_capacity) {
    if (str->capacity >= min_capacity) return 1;

    size_t new_capacity = str->capacity * 2;
    if (new_capacity < min_capacity) new_capacity = min_capacity;

    char* new_data = (char*)realloc(str->data, new_capacity);
    if (!new_data) return 0;

    str->data = new_data;
    str->capacity = new_capacity;
    return 1;
}

void string_shrink_to_fit(String* str) {
    if (str->capacity > str->length + 1) {
        char* new_data = (char*)realloc(str->data, str->length + 1);
        if (new_data) {
            str->data = new_data;
            str->capacity = str->length + 1;
        }
    }
}

const char* string_cstr(const String* str) {
    return str ? str->data : NULL;
}

size_t string_length(const String* str) {
    return str ? str->length : 0;
}

int string_is_empty(const String* str) {
    return !str || str->length == 0;
}

int string_equals(const String* str1, const String* str2) {
    if (!str1 || !str2) return str1 == str2;
    if (str1->length != str2->length) return 0;
    return strcmp(str1->data, str2->data) == 0;
}

// 比较字符串和C字符串的辅助函数
int string_equals_cstr(const String* str, const char* cstr) {
    if (!str || !cstr) return str == NULL && cstr == NULL;
    return strcmp(str->data, cstr) == 0;
}

int string_equals_ignore_case(const String* str1, const String* str2) {
    if (!str1 || !str2) return str1 == str2;
    if (str1->length != str2->length) return 0;

    for (size_t i = 0; i < str1->length; i++) {
        if (tolower(str1->data[i]) != tolower(str2->data[i])) {
            return 0;
        }
    }
    return 1;
}

int string_compare(const String* str1, const String* str2) {
    if (!str1 || !str2) {
        if (!str1 && !str2) return 0;
        return str1 ? 1 : -1;
    }
    return strcmp(str1->data, str2->data);
}

int string_starts_with(const String* str, const String* prefix) {
    if (!str || !prefix) return 0;
    if (prefix->length > str->length) return 0;

    return strncmp(str->data, prefix->data, prefix->length) == 0;
}

int string_ends_with(const String* str, const String* suffix) {
    if (!str || !suffix) return 0;
    if (suffix->length > str->length) return 0;

    return strncmp(str->data + str->length - suffix->length,
                  suffix->data, suffix->length) == 0;
}

void string_append(String* str, const String* other) {
    if (!str || !other) return;

    size_t new_length = str->length + other->length;
    if (!string_ensure_capacity(str, new_length + 1)) return;

    strcat(str->data, other->data);
    str->length = new_length;
}

void string_append_cstr(String* str, const char* cstr) {
    if (!str || !cstr) return;

    size_t cstr_len = strlen(cstr);
    size_t new_length = str->length + cstr_len;
    if (!string_ensure_capacity(str, new_length + 1)) return;

    strcat(str->data, cstr);
    str->length = new_length;
}

void string_append_char(String* str, char c) {
    if (!str) return;

    size_t new_length = str->length + 1;
    if (!string_ensure_capacity(str, new_length + 1)) return;

    str->data[str->length] = c;
    str->data[new_length] = '\0';
    str->length = new_length;
}

void string_append_int(String* str, int value) {
    if (!str) return;

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    string_append_cstr(str, buffer);
}

void string_append_double(String* str, double value, int precision) {
    if (!str) return;

    char format[16];
    snprintf(format, sizeof(format), "%%.%df", precision);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), format, value);
    string_append_cstr(str, buffer);
}

void string_insert(String* str, size_t index, const String* other) {
    if (!str || !other || index > str->length) return;

    size_t new_length = str->length + other->length;
    if (!string_ensure_capacity(str, new_length + 1)) return;

    // 移动现有字符以腾出空间
    memmove(str->data + index + other->length,
            str->data + index,
            str->length - index + 1);

    // 插入新字符串
    memcpy(str->data + index, other->data, other->length);
    str->length = new_length;
}

void string_insert_cstr(String* str, size_t index, const char* cstr) {
    if (!str || !cstr || index > str->length) return;

    size_t cstr_len = strlen(cstr);
    size_t new_length = str->length + cstr_len;
    if (!string_ensure_capacity(str, new_length + 1)) return;

    // 移动现有字符以腾出空间
    memmove(str->data + index + cstr_len,
            str->data + index,
            str->length - index + 1);

    // 插入新字符串
    memcpy(str->data + index, cstr, cstr_len);
    str->length = new_length;
}

void string_remove(String* str, size_t index, size_t count) {
    if (!str || index >= str->length) return;

    if (index + count > str->length) {
        count = str->length - index;
    }

    memmove(str->data + index,
            str->data + index + count,
            str->length - index - count + 1);

    str->length -= count;
}

void string_clear(String* str) {
    if (!str) return;

    str->data[0] = '\0';
    str->length = 0;
}

void string_set(String* str, const char* cstr) {
    if (!str || !cstr) return;

    size_t new_length = strlen(cstr);
    if (!string_ensure_capacity(str, new_length + 1)) return;

    strcpy(str->data, cstr);
    str->length = new_length;
}

void string_set_string(String* dest, const String* src) {
    if (!dest || !src) return;
    string_set(dest, src->data);
}

void string_to_upper(String* str) {
    if (!str) return;

    for (size_t i = 0; i < str->length; i++) {
        str->data[i] = toupper(str->data[i]);
    }
}

void string_to_lower(String* str) {
    if (!str) return;

    for (size_t i = 0; i < str->length; i++) {
        str->data[i] = tolower(str->data[i]);
    }
}

// 优化的字符串替换函数
void string_replace(String* str, const String* old_str, const String* new_str) {
    if (!str || !old_str || !new_str || string_is_empty(old_str)) return;

    // 如果旧字符串不在原字符串中，直接返回
    if (string_index_of_str(str, old_str, 0) == -1) return;

    String* result = string_create_empty();
    if (!result) return;

    size_t pos = 0;
    size_t old_len = old_str->length;
    size_t new_len = new_str->length;

    while (pos < str->length) {
        int found_pos = string_index_of_str(str, old_str, pos);
        if (found_pos == -1) break;

        // 添加旧字符串之前的部分
        if (found_pos > pos) {
            string_append_cstr(result, str->data + pos);
            // 覆盖到found_pos - 1的位置
            result->data[result->length - (found_pos - pos)] = '\0';
            result->length = found_pos - pos;
        }

        // 添加新字符串
        string_append(result, new_str);

        pos = found_pos + old_len;
    }

    // 添加剩余部分
    if (pos < str->length) {
        string_append_cstr(result, str->data + pos);
    }

    // 更新原字符串
    string_set_string(str, result);
    string_destroy(result);
}

void string_replace_cstr(String* str, const char* old_cstr, const char* new_cstr) {
    String* old_str = string_create(old_cstr);
    String* new_str = string_create(new_cstr);

    string_replace(str, old_str, new_str);

    string_destroy(old_str);
    string_destroy(new_str);
}

void string_trim(String* str) {
    string_trim_left(str);
    string_trim_right(str);
}

void string_trim_left(String* str) {
    if (!str) return;

    size_t i = 0;
    while (i < str->length && isspace(str->data[i])) {
        i++;
    }

    if (i > 0) {
        memmove(str->data, str->data + i, str->length - i + 1);
        str->length -= i;
    }
}

void string_trim_right(String* str) {
    if (!str) return;

    size_t i = str->length;
    while (i > 0 && isspace(str->data[i - 1])) {
        i--;
    }

    str->data[i] = '\0';
    str->length = i;
}

String* string_substring(const String* str, size_t start, size_t end) {
    if (!str || start > end || end > str->length) return NULL;

    size_t sub_length = end - start;
    String* result = string_create_empty();

    if (!string_ensure_capacity(result, sub_length + 1)) {
        string_destroy(result);
        return NULL;
    }

    strncpy(result->data, str->data + start, sub_length);
    result->data[sub_length] = '\0';
    result->length = sub_length;

    return result;
}

int string_index_of(const String* str, char c, size_t start) {
    if (!str || start >= str->length) return -1;

    for (size_t i = start; i < str->length; i++) {
        if (str->data[i] == c) return i;
    }
    return -1;
}

int string_index_of_str(const String* str, const String* substr, size_t start) {
    if (!str || !substr || start >= str->length) return -1;

    char* pos = strstr(str->data + start, substr->data);
    if (!pos) return -1;

    return pos - str->data;
}

int string_last_index_of(const String* str, char c, size_t start) {
    if (!str) return -1;

    if (start >= str->length) start = str->length - 1;

    for (size_t i = start; i != (size_t)-1; i--) {
        if (str->data[i] == c) return i;
    }
    return -1;
}

StringArray* string_split(const String* str, const String* delimiter) {
    if (!str || !delimiter || string_is_empty(delimiter)) return NULL;

    StringArray* result = string_array_create();
    size_t start = 0;
    size_t pos = 0;

    while ((pos = string_index_of_str(str, delimiter, start)) != (size_t)-1) {
        String* part = string_substring(str, start, pos);
        string_array_add(result, part);
        string_destroy(part);

        start = pos + delimiter->length;
    }

    // 添加最后一部分
    if (start < str->length) {
        String* part = string_substring(str, start, str->length);
        string_array_add(result, part);
        string_destroy(part);
    }

    return result;
}

// 获取字符串中指定位置的字符
char string_char_at(const String* str, size_t index) {
    if (!str || index >= str->length) {
        return '\0'; // 如果索引越界或字符串为空，返回空字符
    }
    return str->data[index];
}

// 安全获取字符串中指定位置的字符（带错误检查）
int string_char_at_safe(const String* str, size_t index, char* result) {
    if (!str || !result) {
        return 0; // 失败
    }

    if (index >= str->length) {
        return 0; // 索引越界，失败
    }

    *result = str->data[index];
    return 1; // 成功
}

// ==================== 类型转换函数实现 ====================

int string_to_int(const String* str, int* result) {
    if (!str || !result) return 0;

    errno = 0;
    char* endptr;
    long value = strtol(str->data, &endptr, 10);

    // 检查转换是否成功
    if (endptr == str->data || *endptr != '\0' || errno == ERANGE) {
        return 0; // 转换失败
    }

    // 检查是否在int范围内
    if (value > INT_MAX || value < INT_MIN) {
        return 0; // 超出范围
    }

    *result = (int)value;
    return 1; // 成功
}

int string_to_double(const String* str, double* result) {
    if (!str || !result) return 0;

    errno = 0;
    char* endptr;
    double value = strtod(str->data, &endptr);

    // 检查转换是否成功
    if (endptr == str->data || *endptr != '\0' || errno == ERANGE) {
        return 0; // 转换失败
    }

    // 检查是否为无穷大或NaN
    if (isinf(value) || isnan(value)) {
        return 0; // 无效值
    }

    *result = value;
    return 1; // 成功
}

int string_to_float(const String* str, float* result) {
    if (!str || !result) return 0;

    double temp;
    if (!string_to_double(str, &temp)) {
        return 0;
    }

    // 检查是否在float范围内
    if (temp > FLT_MAX || temp < -FLT_MAX) {
        return 0; // 超出范围
    }

    *result = (float)temp;
    return 1; // 成功
}

int string_to_bool(const String* str, int* result) {
    if (!str || !result) return 0;

    // 转换为小写进行比较
    String* lower_str = string_clone(str);
    string_to_lower(lower_str);

    int bool_value = 0;

    // 检查常见真值表示
    if (string_equals_cstr(lower_str, "true") ||
        string_equals_cstr(lower_str, "1") ||
        string_equals_cstr(lower_str, "yes") ||
        string_equals_cstr(lower_str, "on")) {
        bool_value = 1;
    }
    // 检查常见假值表示
    else if (string_equals_cstr(lower_str, "false") ||
             string_equals_cstr(lower_str, "0") ||
             string_equals_cstr(lower_str, "no") ||
             string_equals_cstr(lower_str, "off")) {
        bool_value = 0;
    }
    // 如果是数字，非0为真
    else {
        int num_value;
        if (string_to_int(str, &num_value)) {
            bool_value = (num_value != 0);
        } else {
            string_destroy(lower_str);
            return 0; // 无法确定布尔值
        }
    }

    *result = bool_value;
    string_destroy(lower_str);
    return 1; // 成功
}

char string_to_char(const String* str) {
    if (!str || str->length == 0) return '\0';
    return str->data[0];
}

// ==================== 文件操作函数实现 ====================

int string_write_to_file(const String* str, const char* filename) {
    if (!str || !filename) return 0;

    FILE* file = fopen(filename, "w");
    if (!file) return 0;

    size_t written = fwrite(str->data, sizeof(char), str->length, file);
    fclose(file);

    return (written == str->length);
}

String* string_read_from_file(const char* filename) {
    if (!filename) return NULL;

    FILE* file = fopen(filename, "r");
    if (!file) return NULL;

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return string_create_empty();
    }

    // 创建字符串并读取文件内容
    String* str = string_create_empty();
    if (!str) {
        fclose(file);
        return NULL;
    }

    if (!string_ensure_capacity(str, file_size + 1)) {
        fclose(file);
        string_destroy(str);
        return NULL;
    }

    size_t read_size = fread(str->data, sizeof(char), file_size, file);
    str->data[read_size] = '\0';
    str->length = read_size;

    fclose(file);
    return str;
}

int string_append_to_file(const String* str, const char* filename) {
    if (!str || !filename) return 0;

    FILE* file = fopen(filename, "a");
    if (!file) return 0;

    size_t written = fwrite(str->data, sizeof(char), str->length, file);
    fclose(file);

    return (written == str->length);
}

String* string_format(const char* format, ...) {
    va_list args;
    va_start(args, format);

    // 确定需要的缓冲区大小
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);

    if (needed <= 0) {
        va_end(args);
        return NULL;
    }

    String* str = string_create_empty();
    if (!string_ensure_capacity(str, needed)) {
        va_end(args);
        string_destroy(str);
        return NULL;
    }

    vsnprintf(str->data, str->capacity, format, args);
    str->length = strlen(str->data);
    va_end(args);

    return str;
}

void string_print(const String* str) {
    if (str) {
        printf("%s\n", str->data);
    }
}

// ==================== 字符串数组函数实现 ====================

StringArray* string_array_create() {
    return string_array_create_with_capacity(10);
}

StringArray* string_array_create_with_capacity(size_t capacity) {
    StringArray* array = (StringArray*)malloc(sizeof(StringArray));
    if (!array) return NULL;

    array->items = (String**)malloc(sizeof(String*) * capacity);
    if (!array->items) {
        free(array);
        return NULL;
    }

    array->count = 0;
    array->capacity = capacity;
    return array;
}

void string_array_destroy(StringArray* array) {
    if (array) {
        for (size_t i = 0; i < array->count; i++) {
            string_destroy(array->items[i]);
        }
        free(array->items);
        free(array);
    }
}

void string_array_add(StringArray* array, const String* str) {
    if (!array || !str) return;

    if (array->count >= array->capacity) {
        size_t new_capacity = array->capacity * 2;
        String** new_items = (String**)realloc(array->items, sizeof(String*) * new_capacity);
        if (!new_items) return;

        array->items = new_items;
        array->capacity = new_capacity;
    }

    array->items[array->count] = string_clone(str);
    array->count++;
}

void string_array_add_cstr(StringArray* array, const char* cstr) {
    String* str = string_create(cstr);
    if (str) {
        string_array_add(array, str);
        string_destroy(str);
    }
}

void string_array_insert(StringArray* array, size_t index, const String* str) {
    if (!array || !str || index > array->count) return;

    if (array->count >= array->capacity) {
        size_t new_capacity = array->capacity * 2;
        String** new_items = (String**)realloc(array->items, sizeof(String*) * new_capacity);
        if (!new_items) return;

        array->items = new_items;
        array->capacity = new_capacity;
    }

    // 移动元素以腾出空间
    for (size_t i = array->count; i > index; i--) {
        array->items[i] = array->items[i - 1];
    }

    array->items[index] = string_clone(str);
    array->count++;
}

void string_array_remove(StringArray* array, size_t index) {
    if (!array || index >= array->count) return;

    string_destroy(array->items[index]);

    // 移动元素以填补空隙
    for (size_t i = index; i < array->count - 1; i++) {
        array->items[i] = array->items[i + 1];
    }

    array->count--;
}

void string_array_clear(StringArray* array) {
    if (!array) return;

    for (size_t i = 0; i < array->count; i++) {
        string_destroy(array->items[i]);
    }

    array->count = 0;
}

String* string_array_get(const StringArray* array, size_t index) {
    if (!array || index >= array->count) return NULL;
    return array->items[index];
}

size_t string_array_size(const StringArray* array) {
    return array ? array->count : 0;
}

int string_array_is_empty(const StringArray* array) {
    return !array || array->count == 0;
}

void string_array_print(const StringArray* array) {
    if (!array) return;

    printf("[");
    for (size_t i = 0; i < array->count; i++) {
        printf("\"%s\"", array->items[i]->data);
        if (i < array->count - 1) {
            printf(", ");
        }
    }
    printf("]\n");
}

String* string_array_join(const StringArray* array, const String* separator) {
    if (!array || array->count == 0) return string_create_empty();
    if (!separator) return string_create_empty();

    String* result = string_create_empty();
    if (!result) return NULL;

    for (size_t i = 0; i < array->count; i++) {
        string_append(result, array->items[i]);
        if (i < array->count - 1) {
            string_append(result, separator);
        }
    }

    return result;
}

// ==================== 字符串数组文件操作 ====================

int string_array_write_to_file(const StringArray* array, const char* filename, const char* delimiter) {
    if (!array || !filename) return 0;

    FILE* file = fopen(filename, "w");
    if (!file) return 0;

    const char* delim = delimiter ? delimiter : "\n";
    size_t delim_len = strlen(delim);

    int success = 1;
    for (size_t i = 0; i < array->count; i++) {
        size_t written = fwrite(array->items[i]->data, sizeof(char), array->items[i]->length, file);
        if (written != array->items[i]->length) {
            success = 0;
            break;
        }

        // 写入分隔符（除了最后一个元素）
        if (i < array->count - 1) {
            written = fwrite(delim, sizeof(char), delim_len, file);
            if (written != delim_len) {
                success = 0;
                break;
            }
        }
    }

    fclose(file);
    return success;
}

StringArray* string_array_read_from_file(const char* filename, const char* delimiter) {
    if (!filename) return NULL;

    // 先读取整个文件内容
    String* file_content = string_read_from_file(filename);
    if (!file_content) return NULL;

    // 使用分隔符分割内容
    const char* delim = delimiter ? delimiter : "\n";
    String* delim_str = string_create(delim);
    StringArray* result = string_split(file_content, delim_str);

    string_destroy(file_content);
    string_destroy(delim_str);
    
    return result;
}

// ==================== 控制台输入函数实现 ====================

String* string_read_line() {
    String* str = string_create_empty();
    if (!str) return NULL;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        string_append_cstr(str, buffer);

        // 检查是否读取了完整的一行（以换行符结束）
        if (str->length > 0 && str->data[str->length - 1] == '\n') {
            // 移除末尾的换行符
            str->data[str->length - 1] = '\0';
            str->length--;

            // 如果是Windows风格的换行，也移除回车符
            if (str->length > 0 && str->data[str->length - 1] == '\r') {
                str->data[str->length - 1] = '\0';
                str->length--;
            }
            break;
        }
    }

    // 如果读取失败且字符串为空，释放内存
    if (string_is_empty(str)) {
        string_destroy(str);
        return NULL;
    }

    return str;
}

String* string_read_line_with_prompt(const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);  // 确保提示信息立即显示
    }
    return string_read_line();
}

int string_read_line_into(String* str) {
    if (!str) return 0;

    // 清空目标字符串
    string_clear(str);

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        string_append_cstr(str, buffer);

        // 检查是否读取了完整的一行（以换行符结束）
        if (str->length > 0 && str->data[str->length - 1] == '\n') {
            // 移除末尾的换行符
            str->data[str->length - 1] = '\0';
            str->length--;

            // 如果是Windows风格的换行，也移除回车符
            if (str->length > 0 && str->data[str->length - 1] == '\r') {
                str->data[str->length - 1] = '\0';
                str->length--;
            }
            return 1;  // 成功读取
        }
    }

    return 0;  // 读取失败
}