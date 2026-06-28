#ifndef COMPILER_CODEGEN_H
#define COMPILER_CODEGEN_H

#include "IR.h"
extern bool has_print;
extern bool has_string_lib;
extern bool has_memory_lib;
extern bool has_float_lib;
extern bool has_array_lib;
extern bool main_has_args;
extern bool has_bounds_check;
extern bool has_format;
extern bool has_input;
extern bool has_system_lib;
extern bool has_gc;

#define MAX_TEMPS 256
static char* temp_names[MAX_TEMPS];
static DataType temp_types[MAX_TEMPS];
static int temp_count = 0;

typedef struct {
    char** names;      // 临时变量名
    DataType* types;   // 对应的数据类型
    int count;
    int capacity;
} TempMap;

static TempMap temp_map = {NULL, NULL, 0, 0};

typedef enum StorageClass {
    STORAGE_GLOBAL,
    STORAGE_PARAM,
    STORAGE_LOCAL
} StorageClass;

typedef enum VAR_TYPE
{
    TYPE_CONST,
    TYPE_VAR,
    TYPE_UNINIT_VAR,
}Var_Type;

typedef struct CONST
{
    DataType* type;
    char** value;
    DataType* value_type;
    char** name;
    int index;
    int size;
    Var_Type* var_type;
    bool* is_array;
    int* array_size;

    StorageClass* storage;
    int* offset;
}Const;

typedef struct {
    int  stack_slot;
    bool is_spilled;
} SpillSlot;

void codegen_win32_init();
void codegen_win32_start(FILE* output);
void codegen_win32(FILE* output,IR* ir);
void codegen_win32_end(FILE* output);
int get_const(char* name);
void add_symbol(DataType type, char* value, char* name,
                Var_Type var_type, DataType value_type,
                StorageClass storage, int offset,
                bool is_array, int array_size);
void codegen_cleanup(void);
#endif