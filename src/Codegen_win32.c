#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Codegen.h"
#include "Semantics.h"
#include "Error.h"
#include "regmap.h"

#define MAX_SPILL_SLOTS 64

bool has_print = false;
bool has_string_lib = false;
bool has_memory_lib = false;
bool has_float_lib = false;
bool in_main = false;
bool has_array_lib = false;
bool has_bounds_check = false;
bool has_format = false;
bool has_input = false;
bool has_system_lib = false;
bool has_gc = false;

static char* current_function = NULL;
static int current_param_offset = 8;
static int current_param_index = 0;
static bool in_function = false;
static int current_local_offset = 0;
static SpillSlot spill_slots[MAX_SPILL_SLOTS];
static int spill_count = 0;

int const_i = 0;
int buffer_i = 0;
Const* consts = NULL;

static const char* canonical_name(const char* name);

int alloc_spill_slot(void) {
    if (spill_count >= MAX_SPILL_SLOTS) {
        INTERNAL_ERROR("too many spill slots");
    }
    int slot = -4 - spill_count * 4;
    spill_slots[spill_count++] = (SpillSlot){.stack_slot = slot, .is_spilled = true};
    return slot;
}

static const char* strip_ssa_suffix(const char* name) {
    if (!name) return NULL;

    const char* at = strrchr(name, '@');
    if (!at || at == name) return name;

    const char* p = at + 1;
    if (!*p || !isdigit((unsigned char)*p)) return name;
    while (*p) {
        if (!isdigit((unsigned char)*p)) return name;
        p++;
    }

    static char buf[256];
    size_t len = at - name;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, name, len);
    buf[len] = '\0';
    return buf;
}

static char* escape_for_nasm(const char* str) {
    if (str == NULL) return strdup("");

    size_t alloc = 1;
    for (const char* p = str; *p; p++) {
        if ((unsigned char)*p < 32 || *p == '`')
            alloc += 8;
        else if (*p == '\\' && *(p+1) == 'n') {
            alloc += 8; p++;
        } else if (*p == '\\' && *(p+1) == '"') {
            alloc += 8; p++;
        } else
            alloc += 1;
    }
    alloc += 16;

    char* buf = malloc(alloc);
    if (!buf) return strdup(str);
    char* out = buf;

    for (const char* p = str; *p; p++) {
        if (*p == '\\' && *(p+1) == 'n') {
            out += sprintf(out, "`,10,`");
            p++;
        } else if (*p == '\\' && *(p+1) == '"') {
            out += sprintf(out, "`,34,`");
            p++;
        } else if ((unsigned char)*p >= 32 && *p != '`') {
            *out++ = *p;
        } else if (*p == '`') {
            *out++ = '`'; *out++ = '`';
        } else {
            out += sprintf(out, "`,%d,`", (unsigned char)*p);
        }
    }
    *out = '\0';
    return buf;
}

static bool is_local_label(const char* name) {
    if (!name || !name[0]) return false;
    return (name[0] == 'l' && isdigit((unsigned char)name[1]));
}

char* string_buffer_get()
{
    int len = snprintf(NULL, 0, "buffer%d", temp_i);
    char* temp = malloc(len + 1);
    if (temp) {
        snprintf(temp, len + 1, "buffer%d", ++temp_i);
    }
    return temp;
}

bool is_var_name(char* value)
{
    struct key_token token = find_key(value);
    return token.type == TOKEN_IDENTIFIER;
}

bool is_temp(const char* str)
{
    if (str == NULL) return false;
    return str[0] == '.' && str[1] == 't' && isdigit(str[2]);
}

bool is_var(const char* str)
{
    if (str == NULL) return false;
    return str[0] == '_' && str[1] == 'v' && str[2] == 'a' && str[3] == 'r';
}

bool is_number(char* code)
{
    if (code == NULL || code[0] == '\0') return false;
    char* s = code;
    if (*s == '-') s++;
    if (*s == '\0') return false;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        if (!isxdigit((unsigned char)*s)) return false;
        while (isxdigit((unsigned char)*s)) s++;
        return *s == '\0';
    }

    if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        s += 2;
        while (*s == '0' || *s == '1') s++;
        return *s == '\0';
    }

    if (s[0] == '0' && s[1] != '\0' && s[1] != 'x' && s[1] != 'X'
        && s[1] != 'b' && s[1] != 'B' && s[1] != '.') {
        s++;
        while (*s >= '0' && *s <= '7') s++;
        if (*s == '\0') return true;
        return false;
        }

    bool has_digit = false;
    bool has_dot   = false;
    while (*s != '\0') {
        if (isdigit((unsigned char)*s)) {
            has_digit = true;
        } else if (*s == '.' && !has_dot) {
            has_dot = true;
        } else {
            return false;
        }
        s++;
    }
    return has_digit;
}

char* add_const(DataType type, char* value, int array_size,DataType value_type);

DataType load_var(FILE* out, const char* var_name) {
    if (is_temp(var_name)) {
        int off = (atoi(var_name + 2) + 1) * 4;
        fprintf(out, "  mov eax, dword [ebp-%d]\n", off);
        return DATA_INT;
    }
    if (is_number((char*)var_name)) {
        if (strchr(var_name, '.') != NULL) {
            char* label = add_const(DATA_FLOAT, (char*)var_name, 0, DATA_FLOAT);
            fprintf(out, "  fld dword [%s]\n", label);
            return DATA_FLOAT;
        }
        fprintf(out, "  mov eax, %s\n", var_name);
        return DATA_INT;
    }
    const char* canon = canonical_name(var_name);
    int idx = get_const((char*)canon);
    if (idx == -1) {
        FATAL(ERR_CG_UNKNOWN_VAR, LOC_NONE, "unknown variable '%s'", var_name);
    }
    DataType type = consts->type[idx];
    char addr[100];

    switch (consts->storage[idx]) {
    case STORAGE_GLOBAL: sprintf(addr, "[%s]", canon); break;
    case STORAGE_PARAM:  sprintf(addr, "[ebp+%d]", consts->offset[idx]); break;
    case STORAGE_LOCAL:  sprintf(addr, "[ebp-%d]", -consts->offset[idx]); break;
    }

    if (consts->is_array[idx]) {
        fprintf(out, "  lea eax, %s\n", addr);
        return DATA_INT;
    }

    if (type == DATA_FLOAT || type == DATA_UFLOAT) {
        fprintf(out, "  fld dword %s\n", addr);
    } else if (type == DATA_CHAR || type == DATA_BOOL) {
        fprintf(out, "  movzx eax, byte %s\n", addr);
    } else {
        fprintf(out, "  mov eax, dword %s\n", addr);
    }
    return type;
}

void temp_map_init() {
    temp_map.capacity = 16;
    temp_map.count = 0;
    temp_map.names = malloc(temp_map.capacity * sizeof(char*));
    temp_map.types = malloc(temp_map.capacity * sizeof(DataType));
    if (!temp_map.names || !temp_map.types) {
        OUT_OF_MEMORY();
    }
}

DataType temp_map_get(char* name) {
    if (name == NULL) return DATA_ERROR;

    for (int i = 0; i < temp_map.count; i++) {
        if (strcmp(temp_map.names[i], name) == 0) {
            return temp_map.types[i];
        }
    }

    for (int i = 0; i < consts->index; i++) {
        if (strcmp(consts->name[i], name) == 0) {
            return consts->type[i];
        }
    }

    return DATA_ERROR;
}

void temp_map_cleanup() {
    free(temp_map.names);
    free(temp_map.types);
    temp_map.names = NULL;
    temp_map.types = NULL;
    temp_map.count = 0;
    temp_map.capacity = 0;
}

void temp_map_ensure_capacity() {
    if (temp_map.count >= temp_map.capacity) {
        temp_map.capacity *= 2;
        temp_map.names = realloc(temp_map.names, temp_map.capacity * sizeof(char*));
        temp_map.types = realloc(temp_map.types, temp_map.capacity * sizeof(DataType));
        if (!temp_map.names || !temp_map.types) {
            OUT_OF_MEMORY();
        }
    }
}

void temp_map_add(char* name, DataType type) {
    if (name == NULL) return;
    temp_map_ensure_capacity();
    temp_map.names[temp_map.count] = name;
    temp_map.types[temp_map.count] = type;
    temp_map.count++;
}

void const_resize()
{
    if (consts->index >= consts->size)
    {
        int old_size = consts->size;
        consts->size *= 2;
        consts->type = realloc(consts->type,sizeof(DataType) * consts->size);
        if (consts->type == NULL) { OUT_OF_MEMORY(); }
        consts->value = realloc(consts->value, sizeof(char*) * consts->size);
        if (consts->value == NULL) { OUT_OF_MEMORY(); }
        consts->name = realloc(consts->name, sizeof(char*) * consts->size);
        if (consts->name == NULL) { OUT_OF_MEMORY(); }
        consts->var_type = realloc(consts->var_type, sizeof(Var_Type) * consts->size);
        if (consts->var_type == NULL) { OUT_OF_MEMORY(); }
        consts->value_type = realloc(consts->value_type, sizeof(DataType) * consts->size);
        if (consts->value_type == NULL) { OUT_OF_MEMORY(); }
        consts->storage = realloc(consts->storage, sizeof(StorageClass) * consts->size);
        if (consts->storage == NULL) { OUT_OF_MEMORY(); }
        consts->offset = realloc(consts->offset, sizeof(int) * consts->size);
        if (consts->offset == NULL) { OUT_OF_MEMORY(); }
        consts->is_array = realloc(consts->is_array, sizeof(bool) * consts->size);
        if (consts->is_array == NULL) { OUT_OF_MEMORY(); }
        consts->array_size = realloc(consts->array_size, sizeof(int) * consts->size);
        if (consts->array_size == NULL) { OUT_OF_MEMORY(); }
        for (int i = old_size; i < consts->size; i++) {
            consts->type[i] = DATA_INT;
            consts->value[i] = NULL;
            consts->name[i] = NULL;
            consts->var_type[i] = TYPE_VAR;
            consts->value_type[i] = DATA_INT;
            consts->storage[i] = STORAGE_LOCAL;
            consts->offset[i] = 0;
            consts->is_array[i] = false;
            consts->array_size[i] = 0;
        }
    }
}

void Manual_add_const(DataType type, char* value, int array_size, char* name, Var_Type var_type,DataType value_type)
{
    const_resize();

    consts->type[consts->index] = type;
    consts->value[consts->index] = strdup(value);
    consts->var_type[consts->index] = var_type;
    consts->name[consts->index] = strdup(strip_ssa_suffix(name));
    consts->value_type[consts->index] = value_type;
    consts->is_array[consts->index] = false;
    consts->array_size[consts->index] = 0;
    consts->index++;
}

char* add_const(DataType type, char* value, int array_size,DataType value_type)
{
    const_resize();

    char* const_name = NULL;

    switch(type)
    {
    case DATA_STRING:
        {
            int str_len = snprintf(NULL, 0, "_str%d", const_i);
            const_name = malloc(str_len + 1);
            if (const_name) {
                snprintf(const_name, str_len + 1, "_str%d", const_i++);
            }
            consts->type[consts->index] = type;
            consts->value[consts->index] = strdup(value);
            consts->var_type[consts->index] = TYPE_CONST;
            consts->value_type[consts->index] = value_type;
            consts->name[consts->index++] = strdup(const_name);
        }
        break;

    case DATA_FLOAT:
        {
            int len = snprintf(NULL, 0, "_float%d", const_i);
            const_name = malloc(len + 1);
            if (const_name) {
                snprintf(const_name, len + 1, "_float%d", const_i++);
            }
            consts->type[consts->index] = type;
            char* normalized = strdup(value);
            if (normalized[0] == '.') {
                char* tmp = malloc(strlen(normalized) + 3);
                sprintf(tmp, "0%s", normalized);
                free(normalized);
                normalized = tmp;
            }
            size_t Flen = strlen(normalized);
            if (Flen > 0 && normalized[Flen - 1] == '.') {
                char* tmp = malloc(Flen + 3);
                sprintf(tmp, "%s0", normalized);
                free(normalized);
                normalized = tmp;
            }
            consts->value[consts->index] = normalized ? normalized : strdup(value);
            consts->var_type[consts->index] = TYPE_CONST;
            consts->value_type[consts->index] = value_type;
            consts->name[consts->index] = const_name;
            consts->index++;
            const_resize();
            break;
        }
    case DATA_BOOL:
        {
            int bool_len = snprintf(NULL, 0, "_bool%d", const_i);
            const_name = malloc(bool_len + 1);
            if (const_name) {
                snprintf(const_name, bool_len + 1, "_bool%d", const_i++);
            }
            consts->type[consts->index] = type;
            consts->value[consts->index] = strdup(value);
            consts->var_type[consts->index] = TYPE_VAR;
            consts->value_type[consts->index] = value_type;
            consts->name[consts->index++] = strdup(const_name);
        }
        break;

    case DATA_CHAR:
        {
            int char_len = snprintf(NULL, 0, "_char_%d", const_i);
            const_name = malloc(char_len + 1);
            if (const_name) {
                snprintf(const_name, char_len + 1, "_char_%d", const_i++);
            }
            consts->type[consts->index] = type;
            consts->value[consts->index] = strdup(value);
            consts->var_type[consts->index] = TYPE_VAR;
            consts->value_type[consts->index] = value_type;
            consts->name[consts->index++] = strdup(const_name);
        }
        break;

    case DATA_INT:
        {
            int int_len = snprintf(NULL, 0, "_int%d", const_i);
            const_name = malloc(int_len + 1);
            if (const_name) {
                snprintf(const_name, int_len + 1, "_int%d", const_i++);
            }
            consts->type[consts->index] = type;
            consts->value[consts->index] = normalize_number_str(value);
            consts->var_type[consts->index] = TYPE_VAR;
            consts->value_type[consts->index] = value_type;
            consts->name[consts->index++] = strdup(const_name);
        }
        break;

    default:
        INTERNAL_ERROR("add_const: unsupported data type %d", type);
    }

    if (const_name == NULL) {
        OUT_OF_MEMORY();
    }

    if (consts->value[consts->index-1] == NULL) {
        OUT_OF_MEMORY();
    }

    return const_name;
}

bool value_is_var(char* value)
{
    for (int i = 0; i < consts->index; i++)
    {
        if (strcmp(consts->name[i], value) == 0)
        {
            if (is_var_name(consts->value[i]) && consts->value_type[i] != DATA_STRING && consts->value_type[i] != DATA_CHAR)
            {
                return true;
            }
        }
    }
    return false;
}

int get_const(char* name)
{
    if (name == NULL)
    {
        return -1;
    }
    for (int i = 0; i < consts->index; i++)
    {
        if (strcmp(consts->name[i], name) == 0)
        {
            return i;
        }
    }

    const char* base = strip_ssa_suffix(name);
    if (base != name) {
        for (int i = 0; i < consts->index; i++)
        {
            if (strcmp(consts->name[i], base) == 0)
            {
                return i;
            }
        }
    }

    return -1;
}

static const char* canonical_name(const char* name) {
    if (!name) return name;
    if (is_temp(name)) return name;
    if (is_number(name)) return name;
    int idx = get_const((char*)name);
    if (idx >= 0 && consts->name[idx]) {
        return consts->name[idx];
    }
    return name;
}

void print_var_address(FILE* out, char* var_name) {
    if (is_temp(var_name)) {
        int off = (atoi(var_name + 2) + 1) * 4;
        fprintf(out, "[ebp-%d]", off);
        return;
    }
    const char* canon = canonical_name(var_name);
    int idx = get_const((char*)canon);
    if (idx == -1) {
        FATAL(ERR_CG_UNKNOWN_VAR, LOC_NONE, "unknown variable '%s'", var_name);
    }
    switch (consts->storage[idx]) {
    case STORAGE_GLOBAL:
        fprintf(out, "[%s]", canon);
        break;
    case STORAGE_PARAM:
        fprintf(out, "[ebp+%d]", consts->offset[idx]);
        break;
    case STORAGE_LOCAL:
        fprintf(out, "[ebp-%d]", -consts->offset[idx]);
        break;
    }
}

void load_var_to_eax_or_st0(FILE* out, char* var_name, DataType* loaded_type) {
    if (is_temp(var_name)) {
        int off = (atoi(var_name + 2) + 1) * 4;
        fprintf(out, "  mov eax, dword [ebp-%d]\n", off);
        *loaded_type = DATA_INT;
        return;
    }
    if (is_number(var_name)) {
        if (strchr(var_name, '.') != NULL) {
            char* label = add_const(DATA_FLOAT, var_name, 0, DATA_FLOAT);
            fprintf(out, "  fld dword [%s]\n", label);
            *loaded_type = DATA_FLOAT;
            return;
        }
        fprintf(out, "  mov eax, %s\n", var_name);
        *loaded_type = DATA_INT;
        return;
    }
    const char* canon = canonical_name(var_name);
    int idx = get_const((char*)canon);

    if (idx != -1 && consts->is_array[idx]) {
        char addr[100];
        switch (consts->storage[idx]) {
        case STORAGE_GLOBAL: sprintf(addr, "[%s]", canon); break;
        case STORAGE_PARAM:  sprintf(addr, "[ebp+%d]", consts->offset[idx]); break;
        case STORAGE_LOCAL:  sprintf(addr, "[ebp-%d]", -consts->offset[idx]); break;
        }
        fprintf(out, "  lea eax, %s\n", addr);
        *loaded_type = DATA_INT;
        return;
    }

    DataType actual_type = consts->type[idx];
    *loaded_type = actual_type;

    char addr[100];
    switch (consts->storage[idx]) {
    case STORAGE_GLOBAL: sprintf(addr, "[%s]", canon); break;
    case STORAGE_PARAM:  sprintf(addr, "[ebp+%d]", consts->offset[idx]); break;
    case STORAGE_LOCAL:  sprintf(addr, "[ebp-%d]", -consts->offset[idx]); break;
    }

    if (actual_type == DATA_FLOAT || actual_type == DATA_UFLOAT) {
        fprintf(out, "  fld dword %s\n", addr);
    } else if (actual_type == DATA_CHAR || actual_type == DATA_BOOL) {
        fprintf(out, "  movzx eax, byte %s\n", addr);
    } else {
        fprintf(out, "  mov eax, dword %s\n", addr);
    }
}

DataType get_actual_type(char* name, DataType declared_type) {
    if (declared_type == DATA_IDENTIFIER) {
        int idx = get_const(name);
        if (idx != -1) {
            return consts->type[idx];
        }
    }
    return declared_type;
}

DataType get_type(char* operand, DataType declared) {
    if (is_temp(operand)) {
        DataType t = temp_map_get(operand);
        if (t != DATA_ERROR) return t;
    } else {
        int idx = get_const(operand);
        if (idx != -1) return consts->type[idx];
    }
    return declared;
}

void add_symbol(DataType type, char* value, char* name,
                Var_Type var_type, DataType value_type,
                StorageClass storage, int offset,
                 bool is_array, int array_size)
{
    const_resize();
    int idx = consts->index;
    consts->type[idx] = type;
    consts->value[idx] = strdup(value);
    consts->var_type[idx] = var_type;
    consts->name[idx] = strdup(strip_ssa_suffix(name));
    consts->value_type[idx] = value_type;
    consts->storage[idx] = storage;
    consts->offset[idx] = offset;
    consts->is_array[idx] = is_array;
    consts->array_size[idx] = array_size;
    consts->index++;
}

int get_type_code(DataType type) {
    switch (type) {
    case DATA_INT: return 0;
    case DATA_FLOAT: return 1;
    case DATA_CHAR: return 2;
    case DATA_STRING: return 3;
    case DATA_BOOL: return 4;
    default: return 0;
    }
}

static int map_to_runtime_type(DataType dt) {
    switch (dt) {
    case DATA_INT:    return 0;
    case DATA_FLOAT:  return 1;
    case DATA_CHAR:   return 2;
    case DATA_STRING: return 3;
    case DATA_BOOL:   return 4;
    case DATA_UINT:   return 5;
    case DATA_UFLOAT: return 6;
    default:          return 0;
    }
}

static DataType load_var_regmap(FILE* out, const char* var_name) {
    const RegMapEntry* entry = regmap_get(g_regmap, var_name);

    if (entry != NULL) {
        if (entry->reg != R_NONE && !entry->is_float) {
            fprintf(out, "  ; load '%s' from register %s\n",
                    var_name,
                    (const char*[]){"eax","ecx","edx","ebx","esi","edi"}[entry->reg]);
            if (entry->reg == R_EAX) {
            } else {
                fprintf(out, "  mov eax, %s\n",
                        (const char*[]){"eax","ecx","edx","ebx","esi","edi"}[entry->reg]);
            }
            return DATA_INT;
        }
        else if (entry->reg != R_NONE && entry->is_float) {
            fprintf(out, "  ; load '%s' from x87 stack\n", var_name);
            fprintf(out, "  fld dword [ebp-%d]\n", entry->spill_slot);
            return DATA_FLOAT;
        }
        else {
            fprintf(out, "  ; load '%s' from spill slot [ebp-%d]\n",
                    var_name, entry->spill_slot);
            fprintf(out, "  mov eax, dword [ebp-%d]\n", entry->spill_slot);
            return DATA_INT;
        }
    }

    if (is_temp(var_name)) {
        int off = (atoi(var_name + 2) + 1) * 4;
        DataType t = temp_map_get((char*)var_name);
        if (t == DATA_FLOAT || t == DATA_UFLOAT) {
            fprintf(out, "  fld dword [ebp-%d]\n", off);
            return t;
        }
        fprintf(out, "  mov eax, dword [ebp-%d]\n", off);
        return (t != DATA_ERROR) ? t : DATA_INT;
    }

    int idx = get_const(var_name);
    if (idx == -1) {
        FATAL(ERR_CG_UNKNOWN_VAR, LOC_NONE, "unknown variable '%s'", var_name);
    }
    DataType type = consts->type[idx];
    char addr[100];

    switch (consts->storage[idx]) {
    case STORAGE_GLOBAL: sprintf(addr, "[%s]", var_name); break;
    case STORAGE_PARAM:  sprintf(addr, "[ebp+%d]", consts->offset[idx]); break;
    case STORAGE_LOCAL:  sprintf(addr, "[ebp-%d]", -consts->offset[idx]); break;
    default:             sprintf(addr, "[%s]", var_name); break;
    }

    if (consts->is_array[idx]) {
        fprintf(out, "  lea eax, %s\n", addr);
        return DATA_INT;
    }

    if (type == DATA_FLOAT || type == DATA_UFLOAT) {
        fprintf(out, "  fld dword %s\n", addr);
    } else if (type == DATA_CHAR || type == DATA_BOOL) {
        fprintf(out, "  movzx eax, byte %s\n", addr);
    } else {
        fprintf(out, "  mov eax, dword %s\n", addr);
    }
    return type;
}

static void store_var_regmap(FILE* out, const char* var_name,
                             bool is_float, DataType dt) {
    const RegMapEntry* entry = regmap_get(g_regmap, var_name);

    if (entry != NULL && entry->reg != R_NONE && !entry->is_float) {
        if (entry->reg == R_EAX) {
        } else {
            fprintf(out, "  mov %s, eax\n",
                    (const char*[]){"eax","ecx","edx","ebx","esi","edi"}[entry->reg]);
        }
        return;
    }

    if (entry != NULL && entry->spill_slot > 0) {
        if (is_float)
            fprintf(out, "  fstp dword [ebp-%d]\n", entry->spill_slot);
        else
            fprintf(out, "  mov dword [ebp-%d]\n", entry->spill_slot);
        return;
    }

    if (is_temp(var_name)) {
        int off = (atoi(var_name + 2) + 1) * 4;
        if (is_float || dt == DATA_FLOAT || dt == DATA_UFLOAT)
            fprintf(out, "  fstp dword [ebp-%d]\n", off);
        else
            fprintf(out, "  mov dword [ebp-%d], eax\n", off);
        return;
    }

    int idx = get_const(var_name);
    if (idx == -1) return;

    char addr[100];
    switch (consts->storage[idx]) {
    case STORAGE_GLOBAL: sprintf(addr, "[%s]", var_name); break;
    case STORAGE_PARAM:  sprintf(addr, "[ebp+%d]", consts->offset[idx]); break;
    case STORAGE_LOCAL:  sprintf(addr, "[ebp-%d]", -consts->offset[idx]); break;
    default:             sprintf(addr, "[%s]", var_name); break;
    }

    if (is_float)
        fprintf(out, "  fstp dword %s\n", addr);
    else if (dt == DATA_CHAR || dt == DATA_BOOL)
        fprintf(out, "  mov byte %s, al\n", addr);
    else
        fprintf(out, "  mov dword %s, eax\n", addr);
}

static const char* callee_reg_name(unsigned bit) {
    switch (bit) {
    case CALLEE_SAVED_EBX: return "ebx";
    case CALLEE_SAVED_ESI: return "esi";
    case CALLEE_SAVED_EDI: return "edi";
    default:               return "???";
    }
}

static DataType cg_load(FILE* out, const char* name, DataType hint_type) {
    if (name == NULL) return DATA_INT;

    if (is_number((char*)name)) {
        if (strchr(name, '.') != NULL) {
            char* label = add_const(DATA_FLOAT, (char*)name, 0, DATA_FLOAT);
            fprintf(out, "  fld dword [%s]\n", label);
            return DATA_FLOAT;
        }
        fprintf(out, "  mov eax, %s\n", name);
        return DATA_INT;
    }

    const char* canon = canonical_name(name);

    int ridx = get_const((char*)canon);
    if (ridx != -1 && consts->storage[ridx] == STORAGE_PARAM) {
        DataType type = consts->type[ridx];
        char addr[100];
        snprintf(addr, sizeof(addr), "[ebp+%d]", consts->offset[ridx]);
        if (consts->is_array[ridx]) {
            fprintf(out, "  lea eax, %s\n", addr);
            return DATA_INT;
        }
        if (type == DATA_FLOAT || type == DATA_UFLOAT) {
            fprintf(out, "  fld dword %s\n", addr);
            return type;
        } else if (type == DATA_CHAR || type == DATA_BOOL) {
            fprintf(out, "  movzx eax, byte %s\n", addr);
        } else {
            fprintf(out, "  mov eax, dword %s\n", addr);
        }
        return type;
    }

    const RegMapEntry* e = regmap_get(g_regmap, canon);
    if (e != NULL && e->reg != R_NONE && !e->is_float) {
        if (e->reg == R_EAX) {
        } else {
            fprintf(out, "  mov eax, %s\n",
                    (const char*[]){"eax","ecx","edx","ebx","esi","edi"}[e->reg]);
        }
        return hint_type;
    }
    if (e != NULL && e->is_float && e->reg != R_NONE) {
        fprintf(out, "  fld dword [ebp-%d]\n", e->spill_slot);
        return DATA_FLOAT;
    }
    if (e != NULL && e->reg == R_NONE) {
        fprintf(out, "  mov eax, dword [ebp-%d]\n", e->spill_slot);
        return hint_type;
    }

    if (is_temp(canon)) {
        int off = (atoi(canon + 2) + 1) * 4;
        DataType t = temp_map_get((char*)canon);
        if (t == DATA_ERROR) t = hint_type;

        if (t == DATA_FLOAT || t == DATA_UFLOAT) {
            fprintf(out, "  fld dword [ebp-%d]\n", off);
            return t;
        }
        fprintf(out, "  mov eax, dword [ebp-%d]\n", off);
        return t;
    }

    int idx = get_const((char*)canon);
    if (idx == -1) {
        if (canon[0] == '.' && canon[1] == 't' && isdigit((unsigned char)canon[2])) {
            int off = (atoi(canon + 2) + 1) * 4;
            fprintf(out, "  mov eax, dword [ebp-%d]\n", off);
            return hint_type;
        }
        FATAL(ERR_CG_UNKNOWN_VAR, LOC_NONE, "unknown variable '%s'", name);
    }

    DataType type = consts->type[idx];
    char addr[100];
    switch (consts->storage[idx]) {
    case STORAGE_GLOBAL: snprintf(addr, sizeof(addr), "[%s]", canon); break;
    case STORAGE_PARAM:  snprintf(addr, sizeof(addr), "[ebp+%d]", consts->offset[idx]); break;
    case STORAGE_LOCAL:  snprintf(addr, sizeof(addr), "[ebp-%d]", -consts->offset[idx]); break;
    default:             snprintf(addr, sizeof(addr), "[%s]", canon); break;
    }

    if (consts->is_array[idx]) {
        fprintf(out, "  lea eax, %s\n", addr);
        return DATA_INT;
    }

    if (type == DATA_FLOAT || type == DATA_UFLOAT) {
        fprintf(out, "  fld dword %s\n", addr);
    } else if (type == DATA_CHAR || type == DATA_BOOL) {
        fprintf(out, "  movzx eax, byte %s\n", addr);
    } else {
        fprintf(out, "  mov eax, dword %s\n", addr);
    }
    return type;
}

static void cg_store(FILE* out, const char* name, DataType dt, bool is_float) {
    if (name == NULL) return;

    const char* canon = canonical_name(name);

    if (is_float || dt == DATA_FLOAT || dt == DATA_UFLOAT) {
        if (is_temp(canon)) {
            int off = (atoi(canon + 2) + 1) * 4;
            fprintf(out, "  fstp dword [ebp-%d]\n", off);
        } else {
            int idx = get_const((char*)canon);
            if (idx != -1) {
                char addr[100];
                switch (consts->storage[idx]) {
                case STORAGE_GLOBAL: snprintf(addr, sizeof(addr), "[%s]", canon); break;
                case STORAGE_PARAM:  snprintf(addr, sizeof(addr), "[ebp+%d]", consts->offset[idx]); break;
                case STORAGE_LOCAL:  snprintf(addr, sizeof(addr), "[ebp-%d]", -consts->offset[idx]); break;
                default: return;
                }
                fprintf(out, "  fstp dword %s\n", addr);
            }
        }
        return;
    }

    const RegMapEntry* e = regmap_get(g_regmap, canon);

    if (is_temp(canon)) {
        int off = (atoi(canon + 2) + 1) * 4;
        fprintf(out, "  mov dword [ebp-%d], eax\n", off);

        if (e != NULL && e->reg != R_NONE && !e->is_float && e->reg != R_EAX) {
            fprintf(out, "  mov %s, eax\n",
                    (const char*[]){"eax","ecx","edx","ebx","esi","edi"}[e->reg]);
        }
        return;
    }

    if (e != NULL && e->reg != R_NONE && !e->is_float) {
        if (e->reg != R_EAX) {
            fprintf(out, "  mov %s, eax\n",
                    (const char*[]){"eax","ecx","edx","ebx","esi","edi"}[e->reg]);
        }
        return;
    }

    if (e != NULL && e->reg != R_NONE && e->is_float) {
        int idx = get_const((char*)canon);
        if (idx != -1 && !consts->is_array[idx]) {
            char addr[100];
            switch (consts->storage[idx]) {
            case STORAGE_LOCAL:
                sprintf(addr, "[ebp-%d]", -consts->offset[idx]); break;
            case STORAGE_PARAM:
                sprintf(addr, "[ebp+%d]", consts->offset[idx]); break;
            case STORAGE_GLOBAL:
                sprintf(addr, "[%s]", canon); break;
            default: return;
            }
            fprintf(out, "  fstp dword %s\n", addr);
        }
        return;
    }

    if (e != NULL && e->reg == R_NONE) {
        fprintf(out, "  mov dword [ebp-%d], eax\n", e->spill_slot);
        return;
    }

    int idx = get_const((char*)canon);
    if (idx == -1) {
        if (canon[0] == '.' && canon[1] == 't' && isdigit((unsigned char)canon[2])) {
            int off = (atoi(canon + 2) + 1) * 4;
            fprintf(out, "  mov dword [ebp-%d], eax\n", off);
            return;
        }
        FATAL(ERR_CG_UNKNOWN_VAR, LOC_NONE,
              "cg_store: cannot store to unknown variable '%s'", name);
    }

    char addr[100];
    switch (consts->storage[idx]) {
    case STORAGE_GLOBAL: snprintf(addr, sizeof(addr), "[%s]", canon); break;
    case STORAGE_PARAM:  snprintf(addr, sizeof(addr), "[ebp+%d]", consts->offset[idx]); break;
    case STORAGE_LOCAL:  snprintf(addr, sizeof(addr), "[ebp-%d]", -consts->offset[idx]); break;
    default:             snprintf(addr, sizeof(addr), "[%s]", canon); break;
    }

    if (dt == DATA_CHAR || dt == DATA_BOOL)
        fprintf(out, "  mov byte %s, al\n", addr);
    else
        fprintf(out, "  mov dword %s, eax\n", addr);
}

static bool cg_address(FILE* out, const char* name, char* addr_buf, int buf_len) {
    if (name == NULL) { snprintf(addr_buf, buf_len, "0"); return true; }

    const char* canon = canonical_name(name);

    const RegMapEntry* e = regmap_get(g_regmap, canon);
    if (e != NULL && e->reg != R_NONE) {
        if (e->spill_slot > 0) {
            snprintf(addr_buf, buf_len, "[ebp-%d]", e->spill_slot);
            return true;
        }
        snprintf(addr_buf, buf_len, "0");
        return false;
    }
    if (e != NULL && e->reg == R_NONE) {
        snprintf(addr_buf, buf_len, "[ebp-%d]", e->spill_slot);
        return true;
    }

    if (is_temp(canon)) {
        int off = (atoi(canon + 2) + 1) * 4;
        snprintf(addr_buf, buf_len, "[ebp-%d]", off);
        return true;
    }
    int idx = get_const((char*)canon);
    if (idx == -1) { snprintf(addr_buf, buf_len, "[0]"); return false; }
    switch (consts->storage[idx]) {
    case STORAGE_GLOBAL: snprintf(addr_buf, buf_len, "[%s]", canon); break;
    case STORAGE_PARAM:  snprintf(addr_buf, buf_len, "[ebp+%d]", consts->offset[idx]); break;
    case STORAGE_LOCAL:  snprintf(addr_buf, buf_len, "[ebp-%d]", -consts->offset[idx]); break;
    default:             snprintf(addr_buf, buf_len, "[%s]", canon); break;
    }
    return true;
}

static void cg_load_float(FILE* out, const char* name, DataType hint_type) {
    if (name == NULL) return;

    bool is_int_src = (hint_type == DATA_INT  || hint_type == DATA_UINT ||
                       hint_type == DATA_CHAR || hint_type == DATA_BOOL);

    const RegMapEntry* e = regmap_get(g_regmap, name);
    if (e != NULL) {
        if (e->spill_slot > 0) {
            if (is_int_src) {
                if (hint_type == DATA_UINT) {
                    fprintf(out, "  push 0\n");
                    fprintf(out, "  push dword [ebp-%d]\n", e->spill_slot);
                    fprintf(out, "  fild qword [esp]\n");
                    fprintf(out, "  add esp, 8\n");
                } else {
                    fprintf(out, "  fild dword [ebp-%d]\n", e->spill_slot);
                }
            } else {
                fprintf(out, "  fld dword [ebp-%d]\n", e->spill_slot);
            }
            return;
        }
    }

    if (is_temp(name)) {
        int off = (atoi(name + 2) + 1) * 4;
        if (is_int_src) {
            if (hint_type == DATA_UINT) {
                fprintf(out, "  push 0\n");
                fprintf(out, "  push dword [ebp-%d]\n", off);
                fprintf(out, "  fild qword [esp]\n");
                fprintf(out, "  add esp, 8\n");
            } else {
                fprintf(out, "  fild dword [ebp-%d]\n", off);
            }
        } else {
            fprintf(out, "  fld dword [ebp-%d]\n", off);
        }
        return;
    }

    int idx = get_const(name);
    if (idx != -1) {
        char addr[100];
        switch (consts->storage[idx]) {
        case STORAGE_GLOBAL: snprintf(addr, sizeof(addr), "[%s]", name); break;
        case STORAGE_PARAM:  snprintf(addr, sizeof(addr), "[ebp+%d]", consts->offset[idx]); break;
        case STORAGE_LOCAL:  snprintf(addr, sizeof(addr), "[ebp-%d]", -consts->offset[idx]); break;
        default:             snprintf(addr, sizeof(addr), "[%s]", name); break;
        }
        if (is_int_src) {
            if (hint_type == DATA_UINT) {
                fprintf(out, "  push 0\n");
                fprintf(out, "  push dword %s\n", addr);
                fprintf(out, "  fild qword [esp]\n");
                fprintf(out, "  add esp, 8\n");
            } else {
                fprintf(out, "  fild dword %s\n", addr);
            }
        } else {
            fprintf(out, "  fld dword %s\n", addr);
        }
    }

    char* label = add_const(DATA_FLOAT, (char*)name, 0, DATA_FLOAT);
    fprintf(out, "  fld dword [%s]\n", label);
}

void codegen_win32_init()
{
    consts = calloc(1,sizeof(Const));
    if (consts == NULL) { OUT_OF_MEMORY(); }
    consts->index = 0;
    consts->size = 16;
    consts->type = calloc(consts->size,sizeof(DataType));
    if (consts->type == NULL) { OUT_OF_MEMORY(); }
    consts->value = calloc(consts->size,sizeof(char*));
    if (consts->value == NULL) { OUT_OF_MEMORY(); }
    consts->name = calloc(consts->size,sizeof(char*));
    if (consts->name == NULL) { OUT_OF_MEMORY(); }
    consts->var_type = calloc(consts->size,sizeof(Var_Type));
    if (consts->var_type == NULL) { OUT_OF_MEMORY(); }
    consts->value_type = calloc(consts->size,sizeof(DataType));
    if (consts->value_type == NULL) { OUT_OF_MEMORY(); }
    consts->storage = calloc(consts->size,sizeof(StorageClass));
    if (consts->storage == NULL) { OUT_OF_MEMORY(); }
    consts->offset = calloc(consts->size,sizeof(int));
    if (consts->offset == NULL) { OUT_OF_MEMORY(); }
    consts->is_array = calloc(consts->size,sizeof(bool));
    if (consts->is_array == NULL) { OUT_OF_MEMORY(); }
    consts->array_size = calloc(consts->size,sizeof(int));
    if (consts->array_size == NULL) { OUT_OF_MEMORY(); }
    temp_map_init();
}

void codegen_win32_start(FILE* output)
{
}

void codegen_win32(FILE* output,IR* ir)
{
    if (ir == NULL)
    {
        INTERNAL_ERROR("codegen: NULL IR");
    }

    if (g_regmap) {
        g_regmap->count = 0;
        g_regmap->callee_saved_mask = 0;
    }

    IR* ir_temp = ir;
    static unsigned current_func_callee_mask = 0;

    ir_temp = ir;
    while (ir_temp != NULL)
    {
        if (!in_function) {
            switch (ir_temp->type) {
            case IR_VAR:
            case IR_VAR_UNINIT:
            case IR_ARRAY_DECL:
            case IR_LABEL:
            case IR_PARAM_DEF:
                break;
            default:
                ir_temp = ir_temp->next;
                continue;
            }
        }
        switch (ir_temp->type)
        {
        case IR_LITERAL:
            {
                if (ir_temp->result && ir_temp->args1 &&
                    strcmp(ir_temp->result, ir_temp->args1) == 0) {
                    break;
                    }

                DataType dst_type = ir_temp->data_type;
                char* src = ir_temp->args1;
                DataType src_type = ir_temp->args1_type;

                if (dst_type == DATA_STRING &&
                    !is_temp(src) && get_const(src) == -1) {
                    char* const_label = add_const(DATA_STRING, src, 0, DATA_STRING);
                    fprintf(output, "  mov eax, %s\n", const_label);
                    cg_store(output, ir_temp->result, DATA_STRING, false);
                }
                else if ((dst_type == DATA_FLOAT || dst_type == DATA_UFLOAT) &&
                         !is_temp(src) && get_const(src) == -1) {
                    if (is_number(src) || src[0] == '.' ||
                        (src[0] >= '0' && src[0] <= '9') || src[0] == '-') {
                        char* const_label = add_const(DATA_FLOAT, src, 0, DATA_FLOAT);
                        fprintf(output, "  fld dword [%s]\n", const_label);
                    } else {
                        cg_load(output, src, src_type);
                    }
                    cg_store(output, ir_temp->result, dst_type, true);
                }
                else if (dst_type == DATA_CHAR && !is_temp(src) && get_const(src) == -1) {
                    char ch = src[0];
                    if (src[0] == '\'') ch = src[1];
                    fprintf(output, "  mov eax, %d\n", ch);
                    cg_store(output, ir_temp->result, DATA_CHAR, false);
                }
                else if (dst_type == DATA_BOOL && !is_temp(src) && get_const(src) == -1) {
                    int val = (strcmp(src, "true") == 0 || strcmp(src, "1") == 0) ? 1 : 0;
                    fprintf(output, "  mov eax, %d\n", val);
                    cg_store(output, ir_temp->result, DATA_BOOL, false);
                }
                else if (!is_temp(src) && get_const(src) == -1 && is_number(src)) {
                    fprintf(output, "  mov eax, %s\n", src);
                    cg_store(output, ir_temp->result, dst_type, false);
                }
                else {
                    DataType loaded_type = cg_load(output, src, src_type);
                    if ((dst_type == DATA_FLOAT || dst_type == DATA_UFLOAT)
                        && !(loaded_type == DATA_FLOAT || loaded_type == DATA_UFLOAT))
                    {
                        if (dst_type == DATA_UFLOAT) {
                            fprintf(output, "  push 0\n");
                            fprintf(output, "  push eax\n");
                            fprintf(output, "  fild qword [esp]\n");
                            fprintf(output, "  add esp, 8\n");
                        } else {
                            fprintf(output, "  push eax\n");
                            fprintf(output, "  fild dword [esp]\n");
                            fprintf(output, "  add esp, 4\n");
                        }
                        loaded_type = dst_type;
                    }
                    cg_store(output, ir_temp->result, dst_type,
                             (loaded_type == DATA_FLOAT || loaded_type == DATA_UFLOAT));
                }

                if (ir_temp->result && is_temp(ir_temp->result)) {
                    temp_map_add(ir_temp->result, ir_temp->data_type);
                }
                break;
            }
        case IR_ARRAY_DECL:
            int size = ir_temp->array_size;
            int elem_size = ir_temp->elem_size;
            int total_bytes = size * elem_size;

            current_local_offset -= total_bytes;

            add_symbol(ir_temp->data_type, "", ir_temp->result,
                       TYPE_VAR, ir_temp->data_type,
                       STORAGE_LOCAL, current_local_offset,
                       true, size);
            break;

        case IR_ARRAY_LOAD:
            {
                char* addr_temp = ir_temp->args1;
                int result_offset = (atoi(ir_temp->result + 2) + 1) * 4;
                DataType elem_type = ir_temp->data_type;

                cg_load(output, addr_temp, DATA_INT);

                switch (elem_type) {
                case DATA_CHAR:
                case DATA_BOOL:
                    fprintf(output, "  movzx eax, byte [eax]\n");
                    break;
                case DATA_INT:
                case DATA_UINT:
                case DATA_STRING:
                case DATA_IDENTIFIER:
                    fprintf(output, "  mov eax, [eax]\n");
                    break;
                case DATA_FLOAT:
                case DATA_UFLOAT:
                    fprintf(output, "  fld dword [eax]\n");
                    cg_store(output, ir_temp->result, elem_type, true);
                    if (ir_temp->result && is_temp(ir_temp->result)) {
                        temp_map_add(ir_temp->result, ir_temp->data_type);
                    }
                    break;
                default:
                    INTERNAL_ERROR("codegen: unsupported array element type");
                }
                if (elem_type != DATA_FLOAT && elem_type != DATA_UFLOAT) {
                    cg_store(output, ir_temp->result, elem_type, false);
                    if (ir_temp->result && is_temp(ir_temp->result)) {
                        temp_map_add(ir_temp->result, ir_temp->data_type);
                    }
                }
                break;
            }

        case IR_ARRAY_STORE:
            {
                char* addr_temp = ir_temp->args1;
                char* val_temp = ir_temp->args2;
                DataType elem_type = ir_temp->data_type;

                cg_load(output, addr_temp, DATA_INT);
                fprintf(output, "  mov ecx, eax\n");
                DataType val_loaded_type = cg_load(output, val_temp, elem_type);

                switch (elem_type) {
                case DATA_CHAR:
                case DATA_BOOL:
                    fprintf(output, "  mov [ecx], al\n");
                    break;
                case DATA_INT:
                case DATA_UINT:
                case DATA_STRING:
                case DATA_IDENTIFIER:
                    fprintf(output, "  mov [ecx], eax\n");
                    break;
                case DATA_FLOAT:
                case DATA_UFLOAT:
                    fprintf(output, "  fstp dword [ecx]\n");
                    break;
                default:
                    INTERNAL_ERROR("codegen: unsupported array element type");
                }
                break;
            }

        case IR_ALLOC_ARRAY: {
                char* size_val = ir_temp->args1;
                int elem_size = ir_temp->elem_size;

                if (is_number(size_val)) {
                    fprintf(output, "  mov eax, %s\n", size_val);
                } else if (is_temp(size_val)) {
                    int size_offset = (atoi(size_val + 2) + 1) * 4;
                    fprintf(output, "  mov eax, [ebp-%d]\n", size_offset);
                } else {
                    cg_load(output, size_val, DATA_INT);
                }
                fprintf(output, "  mov ebx, %d\n", elem_size);
                fprintf(output, "  mul ebx\n");
                fprintf(output, "  add eax, 4\n");
                fprintf(output, "  push eax\n");
                fprintf(output, "  call _gc_alloc\n");
                fprintf(output, "  add esp, 4\n");
                fprintf(output, "  mov edx, eax\n");
                if (is_number(size_val)) {
                    fprintf(output, "  mov ebx, %s\n", size_val);
                } else if (is_temp(size_val)) {
                    int size_offset = (atoi(size_val + 2) + 1) * 4;
                    fprintf(output, "  mov ebx, [ebp-%d]\n", size_offset);
                } else {
                    cg_load(output, size_val, DATA_INT);
                    fprintf(output, "  mov ebx, eax\n");
                }
                fprintf(output, "  mov [edx], ebx\n");
                fprintf(output, "  lea eax, [edx + 4]\n");
                cg_store(output, ir_temp->result, DATA_INT, false);
                has_memory_lib = true;
                has_gc = true;
                break;
        }

        case IR_ARRAY_LENGTH: {
                char* arr_name = ir_temp->args1;

                if (is_temp(arr_name)) {
                    int off = (atoi(arr_name + 2) + 1) * 4;
                    fprintf(output, "  mov eax, dword [ebp-%d]\n", off);
                    fprintf(output, "  mov eax, [eax - 4]\n");
                    cg_store(output, ir_temp->result, DATA_INT, false);
                    has_memory_lib = true;
                    break;
                }

                int idx = get_const(arr_name);
                if (idx == -1) {
                    FATAL(ERR_CG_UNKNOWN_VAR, LOC_NONE, "array '%s' not found", arr_name);
                }

                if (consts->is_array[idx]) {
                    int total_size = consts->array_size[idx];
                    fprintf(output, "  mov eax, %d\n", total_size);
                } else {
                    char addr[100];
                    switch (consts->storage[idx]) {
                    case STORAGE_LOCAL:
                        sprintf(addr, "[ebp-%d]", -consts->offset[idx]);
                        break;
                    case STORAGE_PARAM:
                        sprintf(addr, "[ebp+%d]", consts->offset[idx]);
                        break;
                    case STORAGE_GLOBAL:
                        sprintf(addr, "[%s]", arr_name);
                        break;
                    }
                    fprintf(output, "  mov eax, %s\n", addr);
                    fprintf(output, "  mov eax, [eax - 4]\n");
                }

                cg_store(output, ir_temp->result, DATA_INT, false);
                has_memory_lib = true;
                break;
        }

        case IR_ARRAY_TOSTRING: {
                has_array_lib = true;
                has_memory_lib = true;
                has_print = true;
                int result_offset = (atoi(ir_temp->result + 2) + 1) * 4;
                int type_code = get_type_code(ir_temp->args1_type);
                int base_off = (atoi(ir_temp->args1 + 2) + 1) * 4;
                fprintf(output, "  mov eax, [ebp-%d]\n", base_off);

                if (is_number(ir_temp->args2)) {
                    int len = atoi(ir_temp->args2);
                    fprintf(output, "  push %d\n", type_code);
                    fprintf(output, "  push %d\n", len);
                } else {
                    int idx = get_const(ir_temp->args2);
                    if (idx != -1 && consts->is_array[idx]) {
                        fprintf(output, "  push %d\n", type_code);
                        fprintf(output, "  push %d\n", consts->array_size[idx]);
                    } else {
                        fprintf(output, "  push %d\n", type_code);
                        fprintf(output, "  push dword [eax - 4]\n");
                    }
                }
                fprintf(output, "  push eax\n");
                fprintf(output, "  call _array_to_string\n");
                fprintf(output, "  add esp, 12\n");
                cg_store(output, ir_temp->result, DATA_STRING, false);
                has_string_lib = true;
                break;
        }

        case IR_PUSH: {
            char* operand = ir_temp->args1;
            if (operand && operand[0] == '.' && operand[1] == 'd'
                && isdigit((unsigned char)operand[2])) {
                fprintf(output, "  push 0  ; dead value %s\n", operand);
                break;
            }
            DataType type = ir_temp->args1_type;

            bool is_float_push = (type == DATA_FLOAT || type == DATA_UFLOAT);

            if (is_float_push) {
                if (is_temp(operand)) {
                    int off = (atoi(operand + 2) + 1) * 4;
                    fprintf(output, "  push dword [ebp-%d]\n", off);
                } else {
                    int idx = get_const(operand);
                    if (idx != -1) {
                        char addr[100];
                        switch (consts->storage[idx]) {
                        case STORAGE_PARAM:
                            snprintf(addr, sizeof(addr), "[ebp+%d]", consts->offset[idx]);
                            break;
                        case STORAGE_LOCAL:
                            snprintf(addr, sizeof(addr), "[ebp-%d]", -consts->offset[idx]);
                            break;
                        case STORAGE_GLOBAL:
                        default:
                            snprintf(addr, sizeof(addr), "[%s]", operand);
                            break;
                        }
                        fprintf(output, "  push dword %s\n", addr);
                    } else {
                        char* label = add_const(DATA_FLOAT, operand, 0, DATA_FLOAT);
                        fprintf(output, "  push dword [%s]\n", label);
                    }
                }
                break;
            }

            const RegMapEntry* e = regmap_get(g_regmap, operand);
            if (e != NULL && e->reg != R_NONE && !e->is_float) {
                fprintf(output, "  push %s\n",
                        (const char*[]){"eax","ecx","edx","ebx","esi","edi"}[e->reg]);
            } else if (e != NULL && e->reg == R_NONE) {
                fprintf(output, "  push dword [ebp-%d]\n", e->spill_slot);
            } else if (is_temp(operand)) {
                int offset = (atoi(operand + 2) + 1) * 4;
                fprintf(output, "  push dword [ebp-%d]\n", offset);
            } else {
                int idx = get_const(operand);
                if (idx != -1) {
                    switch (consts->storage[idx]) {
                    case STORAGE_PARAM:
                        fprintf(output, "  push dword [ebp+%d]\n", consts->offset[idx]);
                        break;
                    case STORAGE_LOCAL:
                        fprintf(output, "  push dword [ebp-%d]\n", -consts->offset[idx]);
                        break;
                    default:
                        fprintf(output, "  push dword [%s]\n", operand);
                        break;
                    }
                } else {
                    if (type == DATA_FLOAT || type == DATA_UFLOAT) {
                        char* label = add_const(DATA_FLOAT, operand, 0, DATA_FLOAT);
                        fprintf(output, "  push dword [%s]\n", label);
                    } else {
                        fprintf(output, "  push %s\n", operand);
                    }
                }
            }
            break;
        }

            case IR_BINARY: {
                DataType res_type = ir_temp->data_type;
                if (ir_temp->op == IR_JE || ir_temp->op == IR_JNE ||
                        ir_temp->op == IR_JG || ir_temp->op == IR_JL ||
                        ir_temp->op == IR_JGE || ir_temp->op == IR_JLE ||
                        ir_temp->op == IR_ULT || ir_temp->op == IR_ULE ||
                        ir_temp->op == IR_UGT || ir_temp->op == IR_UGE) {
                    res_type = DATA_INT;
                        }
                temp_map_add(ir_temp->result, res_type);

                bool is_float_op = (res_type == DATA_FLOAT || res_type == DATA_UFLOAT) ||
                                   (ir_temp->args1_type == DATA_FLOAT || ir_temp->args1_type == DATA_UFLOAT) ||
                                   (ir_temp->args2_type == DATA_FLOAT || ir_temp->args2_type == DATA_UFLOAT);

                if (is_float_op)
                {
                    cg_load_float(output, ir_temp->args1, ir_temp->args1_type);
                    cg_load_float(output, ir_temp->args2, ir_temp->args2_type);

                    bool float_result_is_int = true;
                    switch (ir_temp->op) {
                    case IR_ADD:
                        fprintf(output, "  faddp\n");
                        float_result_is_int = false;
                        break;
                    case IR_SUB:
                        fprintf(output, "  fsubp\n");
                        float_result_is_int = false;
                        break;
                    case IR_MUL:
                        fprintf(output, "  fmulp\n");
                        float_result_is_int = false;
                        break;
                    case IR_DIV:
                    case IR_UDIV:
                        fprintf(output, "  fdivp\n");
                        float_result_is_int = false;
                        break;
                    case IR_MOD:
                    case IR_UMOD:
                        has_float_lib = true;
                        fprintf(output, "  call _fmod_fpu\n");
                        float_result_is_int = false;
                        break;
                    case IR_JE:
                        has_float_lib = true;
                        fprintf(output, "  sub esp, 8\n");
                        fprintf(output, "  fstp dword [esp+4]\n");
                        fprintf(output, "  fstp dword [esp]\n");
                        fprintf(output, "  call _feq\n");
                        fprintf(output, "  add esp, 8\n");
                        break;
                    case IR_JNE:
                        has_float_lib = true;
                        fprintf(output, "  sub esp, 8\n");
                        fprintf(output, "  fstp dword [esp+4]\n");
                        fprintf(output, "  fstp dword [esp]\n");
                        fprintf(output, "  call _fne\n");
                        fprintf(output, "  add esp, 8\n");
                        break;
                    case IR_JG:
                        has_float_lib = true;
                        fprintf(output, "  sub esp, 8\n");
                        fprintf(output, "  fstp dword [esp+4]\n");
                        fprintf(output, "  fstp dword [esp]\n");
                        fprintf(output, "  call _fgt\n");
                        fprintf(output, "  add esp, 8\n");
                        break;
                    case IR_JGE:
                        has_float_lib = true;
                        fprintf(output, "  sub esp, 8\n");
                        fprintf(output, "  fstp dword [esp+4]\n");
                        fprintf(output, "  fstp dword [esp]\n");
                        fprintf(output, "  call _fge\n");
                        fprintf(output, "  add esp, 8\n");
                        break;
                    case IR_JL:
                        has_float_lib = true;
                        fprintf(output, "  sub esp, 8\n");
                        fprintf(output, "  fstp dword [esp+4]\n");
                        fprintf(output, "  fstp dword [esp]\n");
                        fprintf(output, "  call _flt\n");
                        fprintf(output, "  add esp, 8\n");
                        break;
                    case IR_JLE:
                        has_float_lib = true;
                        fprintf(output, "  sub esp, 8\n");
                        fprintf(output, "  fstp dword [esp+4]\n");
                        fprintf(output, "  fstp dword [esp]\n");
                        fprintf(output, "  call _fle\n");
                        fprintf(output, "  add esp, 8\n");
                        break;
                    default:
                        break;
                    }

                    cg_store(output, ir_temp->result, res_type, !float_result_is_int);
                }
                else
                {
                    if (ir_temp->args2_type == DATA_STRING &&
                                ir_temp->args1_type != DATA_STRING) {
                                cg_load(output, ir_temp->args2, ir_temp->args2_type);
                                fprintf(output, "  push eax\n");

                                cg_load(output, ir_temp->args1, ir_temp->args1_type);
                            } else {
                                DataType right_loaded = cg_load(output, ir_temp->args2, ir_temp->args2_type);
                                fprintf(output, "  push eax\n");
                                cg_load(output, ir_temp->args1, ir_temp->args1_type);
                            }
                            switch (ir_temp->op) {
                            case IR_ADD:
                                fprintf(output, "  add eax, dword [esp]\n");
                                break;
                            case IR_SUB:
                                fprintf(output, "  sub eax, dword [esp]\n");
                                break;
                            case IR_MUL:
                                fprintf(output, "  imul eax, dword [esp]\n");
                                break;
                            case IR_DIV:
                                fprintf(output, "  push edx\n");
                                fprintf(output, "  cdq\n");
                                fprintf(output, "  idiv dword [esp+4]\n");
                                fprintf(output, "  pop edx\n");
                                break;
                            case IR_MOD:
                                fprintf(output, "  push edx\n");
                                fprintf(output, "  cdq\n");
                                fprintf(output, "  idiv dword [esp+4]\n");
                                fprintf(output, "  mov eax, edx\n");
                                fprintf(output, "  pop edx\n");
                                break;
                            case IR_UDIV:
                                fprintf(output, "  push edx\n");
                                fprintf(output, "  xor edx, edx\n");
                                fprintf(output, "  div dword [esp+4]\n");
                                fprintf(output, "  pop edx\n");
                                break;
                            case IR_UMOD:
                                fprintf(output, "  push edx\n");
                                fprintf(output, "  xor edx, edx\n");
                                fprintf(output, "  div dword [esp+4]\n");
                                fprintf(output, "  mov eax, edx\n");
                                fprintf(output, "  pop edx\n");
                                break;
                            case IR_JE:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  sete al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_JNE:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  setne al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_JG:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  setg al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_JL:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  setl al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_JGE:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  setge al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_JLE:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  setle al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_ULT:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  setb al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_ULE:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  setbe al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_UGT:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  seta al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_UGE:
                                fprintf(output, "  cmp eax, dword [esp]\n");
                                fprintf(output, "  setae al\n");
                                fprintf(output, "  movzx eax, al\n");
                                break;
                            case IR_BITWISE_AND:
                                fprintf(output, "  and eax, dword [esp]\n");
                                break;
                            case IR_BITWISE_OR:
                                fprintf(output, "  or eax, dword [esp]\n");
                                break;
                            case IR_SHL:
                                fprintf(output, "  mov ecx, dword [esp]\n");
                                fprintf(output, "  shl eax, cl\n");
                                break;
                            case IR_SHR:
                                fprintf(output, "  mov ecx, dword [esp]\n");
                                fprintf(output, "  shr eax, cl\n");
                                break;
                            case IR_SAR:
                                fprintf(output, "  mov ecx, dword [esp]\n");
                                fprintf(output, "  sar eax, cl\n");
                                break;
                            case IR_BITWISE_XOR:
                                fprintf(output, "  xor eax, dword [esp]\n");
                                break;
                            default:
                                break;
                            }
                            fprintf(output, "  add esp, 4\n");
                            cg_store(output, ir_temp->result, res_type, false);
                        }
                        break;
            }

        case IR_BITWISE_NOT: {
                char* operand = ir_temp->args1;
                DataType type = ir_temp->args1_type;
                int result_offset = (atoi(ir_temp->result + 2) + 1) * 4;

                if (is_temp(operand)) {
                    cg_store(output, ir_temp->result, ir_temp->data_type, false);
                } else if (get_const(operand) != -1) {
                    load_var(output, operand);
                } else {
                    fprintf(output, "  mov eax, %s\n", operand);
                }

                fprintf(output, "  not eax\n");
                fprintf(output, "  mov dword [ebp-%d], eax\n", result_offset);
                break;
        }

        case IR_PRINT:
            {
                char* print_operand = ir_temp->args1;
                if (!ir_temp->call_func_args_type || ir_temp->call_func_args_num < 1) {
                    fprintf(output, "  ; WARNING: malformed IR_PRINT, skipping\n");
                    break;
                }
                DataType print_type = ir_temp->call_func_args_type[0];

                fprintf(output, "  push %d\n", map_to_runtime_type(print_type));

                if (is_temp(print_operand) || get_const(print_operand) != -1) {
                    cg_load(output, print_operand, print_type);
                    if (print_type == DATA_FLOAT || print_type == DATA_UFLOAT) {
                        fprintf(output, "  sub esp, 4\n");
                        fprintf(output, "  fstp dword [esp]\n");
                    } else {
                        fprintf(output, "  push eax\n");
                    }
                }
                else if (print_type == DATA_STRING) {
                    char* str_label = add_const(DATA_STRING, print_operand, 0, DATA_STRING);
                    fprintf(output, "  push %s\n", str_label);
                }
                else if (print_type == DATA_FLOAT || print_type == DATA_UFLOAT) {
                    char* label = add_const(DATA_FLOAT, print_operand, 0, DATA_FLOAT);
                    fprintf(output, "  push dword [%s]\n", label);
                }
                else {
                    fprintf(output, "  push %s\n", print_operand);
                }

                fprintf(output, "  call _print\n");
                fprintf(output, "  add esp, 8\n");
                has_print = true;
                break;
            }

        case IR_INPUT: {
                has_input = true;
                has_memory_lib = true;
                int num = ir_temp->call_func_args_num;
                char* tpl = ir_temp->args1;
                int tpl_off = (atoi(tpl + 2) + 1) * 4;

                fprintf(output, "  sub esp, %d\n", num * 8);
                for (int i = 0; i < num; i++) {
                    DataType t = ir_temp->call_func_args_type[i];
                    int code = (t == DATA_FLOAT || t == DATA_UFLOAT) ? 1 :
                               (t == DATA_CHAR) ? 2 : (t == DATA_STRING) ? 3 :
                               (t == DATA_BOOL) ? 4 : 0;
                    fprintf(output, "  mov dword [esp+%d], %d\n", i*4, code);

                    int idx = get_const(ir_temp->label[i]);
                    if (idx != -1) {
                        char addr[100];
                        switch (consts->storage[idx]) {
                        case STORAGE_GLOBAL: sprintf(addr, "%s", ir_temp->label[i]); break;
                        case STORAGE_PARAM:  sprintf(addr, "ebp+%d", consts->offset[idx]); break;
                        case STORAGE_LOCAL:  sprintf(addr, "ebp-%d", -consts->offset[idx]); break;
                        }
                        fprintf(output, "  lea eax, [%s]\n", addr);
                        fprintf(output, "  mov [esp+%d], eax\n", num*4 + i*4);
                    }
                }
                fprintf(output, "  mov eax, esp\n");
                fprintf(output, "  lea ecx, [esp+%d]\n", num*4);
                fprintf(output, "  push ecx\n");
                fprintf(output, "  push eax\n");
                fprintf(output, "  push %d\n", num);
                fprintf(output, "  push dword [ebp-%d]\n", tpl_off);
                fprintf(output, "  call _input_parse\n");
                fprintf(output, "  add esp, 16\n");
                fprintf(output, "  add esp, %d\n", num * 8);
                break;
        }

        case IR_VAR:
            {
                char* var_name = ir_temp->result;
                DataType var_type = ir_temp->data_type;
                char* src = ir_temp->args1;
                DataType src_type = ir_temp->args1_type;
                int dest_idx = get_const(var_name);

                if (dest_idx == -1) {
                    if (in_function) {
                        current_local_offset -= 4;
                        add_symbol(var_type, "", var_name, TYPE_VAR, src_type,
                                   STORAGE_LOCAL, current_local_offset, false, 0);
                    } else {
                        add_symbol(var_type, src, var_name, TYPE_VAR, src_type,
                                   STORAGE_GLOBAL, 0, false, 0);
                        break;
                    }
                }
                dest_idx = get_const(var_name);
                if (dest_idx == -1) {
                    FATAL(ERR_CG_UNKNOWN_VAR, LOC_NONE,
                          "variable '%s' not in symbol table", var_name);
                }
                if (consts->storage[dest_idx] == STORAGE_GLOBAL) {
                    break;
                }

                if (src != NULL) {
                    if (strcmp(src, "false") == 0 || strcmp(src, "true") == 0) {
                        int val = (strcmp(src, "true") == 0) ? 1 : 0;
                        fprintf(output, "  mov eax, %d\n", val);
                        cg_store(output, var_name, var_type, false);
                        break;
                    }

                    if (var_type == DATA_STRING && !is_temp(src) &&
                        get_const(src) == -1) {
                        char* const_label = add_const(DATA_STRING, src, 0, DATA_STRING);
                        fprintf(output, "  mov eax, %s\n", const_label);
                        cg_store(output, var_name, DATA_STRING, false);
                        break;
                        }

                    if ((var_type == DATA_FLOAT || var_type == DATA_UFLOAT) &&
                        !is_temp(src) && get_const(src) == -1) {
                        if (is_number(src) || src[0] == '.' ||
                            (src[0] >= '0' && src[0] <= '9') || src[0] == '-') {
                            char* const_label = add_const(DATA_FLOAT, src, 0, DATA_FLOAT);
                            fprintf(output, "  fld dword [%s]\n", const_label);
                            cg_store(output, var_name, var_type, true);
                            break;
                            }
                        }

                    if (var_type == DATA_CHAR && !is_temp(src) &&
                        get_const(src) == -1 && !is_number(src)) {
                        char ch = src[0];
                        if (src[0] == '\'') ch = src[1];
                        fprintf(output, "  mov eax, %d\n", ch);
                        cg_store(output, var_name, DATA_CHAR, false);
                        break;
                        }
                }

                DataType loaded_type = cg_load(output, src, src_type);

                if ((var_type == DATA_FLOAT || var_type == DATA_UFLOAT)
                    && !(loaded_type == DATA_FLOAT || loaded_type == DATA_UFLOAT))
                {
                    if (var_type == DATA_UFLOAT) {
                        fprintf(output, "  push 0\n");
                        fprintf(output, "  push eax\n");
                        fprintf(output, "  fild qword [esp]\n");
                        fprintf(output, "  add esp, 8\n");
                    } else {
                        fprintf(output, "  push eax\n");
                        fprintf(output, "  fild dword [esp]\n");
                        fprintf(output, "  add esp, 4\n");
                    }
                    loaded_type = var_type;
                }

                cg_store(output, var_name, var_type,
                         (loaded_type == DATA_FLOAT || loaded_type == DATA_UFLOAT));
                break;
            }

        case IR_VAR_ASS:
            {
                char* dest = ir_temp->result;
                char* src = ir_temp->args1;
                DataType src_type = ir_temp->args1_type;

                int dest_idx = get_const(dest);
                if (dest_idx == -1) {
                    if (in_function) {
                        current_local_offset -= 4;
                        add_symbol(ir_temp->data_type, "", dest, TYPE_VAR, ir_temp->data_type,
                                   STORAGE_LOCAL, current_local_offset, false, 0);
                    } else {
                        add_symbol(ir_temp->data_type, "", dest, TYPE_VAR, ir_temp->data_type,
                                   STORAGE_GLOBAL, 0, false, 0);
                    }
                    dest_idx = get_const(dest);
                    if (dest_idx == -1) {
                        FATAL(ERR_CG_UNKNOWN_VAR, LOC_NONE,
                              "variable '%s' not in symbol table after lazy creation", dest);
                    }
                }
                DataType dest_type = consts->type[dest_idx];

                if (src != NULL) {
                    if (strcmp(src, "false") == 0 || strcmp(src, "true") == 0) {
                        int val = (strcmp(src, "true") == 0) ? 1 : 0;
                        fprintf(output, "  mov eax, %d\n", val);
                        cg_store(output, dest, dest_type, false);
                        break;
                    }
                    if (dest_type == DATA_STRING && !is_temp(src) &&
                        get_const(src) == -1) {
                        char* const_label = add_const(DATA_STRING, src, 0, DATA_STRING);
                        fprintf(output, "  mov eax, %s\n", const_label);
                        cg_store(output, dest, DATA_STRING, false);
                        break;
                        }
                    if ((dest_type == DATA_FLOAT || dest_type == DATA_UFLOAT) &&
                        !is_temp(src) && get_const(src) == -1) {
                        if (is_number(src) || src[0] == '.' ||
                            (src[0] >= '0' && src[0] <= '9') || src[0] == '-') {
                            char* const_label = add_const(DATA_FLOAT, src, 0, DATA_FLOAT);
                            fprintf(output, "  fld dword [%s]\n", const_label);
                            cg_store(output, dest, dest_type, true);
                            break;
                            }
                        }
                    if (dest_type == DATA_CHAR && !is_temp(src) &&
                        get_const(src) == -1 && !is_number(src)) {
                        char ch = src[0];
                        if (src[0] == '\'') ch = src[1];
                        fprintf(output, "  mov eax, %d\n", ch);
                        cg_store(output, dest, DATA_CHAR, false);
                        break;
                        }
                }

                DataType loaded_type = cg_load(output, src, src_type);

                if ((dest_type == DATA_FLOAT || dest_type == DATA_UFLOAT)
                    && !(loaded_type == DATA_FLOAT || loaded_type == DATA_UFLOAT))
                {
                    if (dest_type == DATA_UFLOAT) {
                        fprintf(output, "  push 0\n");
                        fprintf(output, "  push eax\n");
                        fprintf(output, "  fild qword [esp]\n");
                        fprintf(output, "  add esp, 8\n");
                    } else {
                        fprintf(output, "  push eax\n");
                        fprintf(output, "  fild dword [esp]\n");
                        fprintf(output, "  add esp, 4\n");
                    }
                    loaded_type = dest_type;
                }

                cg_store(output, dest, dest_type,
                         (loaded_type == DATA_FLOAT || loaded_type == DATA_UFLOAT));
                break;
            }
        case IR_VAR_UNINIT:
            if (in_function) {
                Manual_add_const(ir_temp->data_type, "", 0, ir_temp->result,
                                 TYPE_UNINIT_VAR, ir_temp->data_type);
                int idx = get_const(ir_temp->result);
                if (idx != -1) {
                    consts->storage[idx] = STORAGE_LOCAL;
                    current_local_offset -= 4;
                    consts->offset[idx] = current_local_offset;
                }
            } else {
                Manual_add_const(ir_temp->data_type, "", 0, ir_temp->result,
                                 TYPE_UNINIT_VAR, ir_temp->data_type);
                int idx = get_const(ir_temp->result);
                if (idx != -1) {
                    consts->storage[idx] = STORAGE_GLOBAL;
                    consts->offset[idx] = 0;
                }
            }
            break;

        case IR_IF:
            break;

        case IR_JMPZ:
            char PZaddr_buf[100];
            if (cg_address(output, ir_temp->args1, PZaddr_buf, sizeof(PZaddr_buf))) {
                fprintf(output, "  cmp DWORD %s, 0\n", PZaddr_buf);
            } else {
                cg_load(output, ir_temp->args1, DATA_INT);
                fprintf(output, "  cmp eax, 0\n");
            }
            if (is_local_label(ir_temp->args2))
                fprintf(output, "  je .%s\n", ir_temp->args2);
            else
                fprintf(output, "  je %s\n", ir_temp->args2);
            break;

        case IR_JMPNZ:
            if (ir_temp->args1 == NULL) {
                INTERNAL_ERROR("codegen: IR_JMPNZ with NULL args1");
            }
            char PNZaddr_buf[100];
            if (cg_address(output, ir_temp->args1, PNZaddr_buf, sizeof(PNZaddr_buf))) {
                fprintf(output, "  cmp DWORD %s, 0\n", PNZaddr_buf);
            } else {
                cg_load(output, ir_temp->args1, DATA_INT);
                fprintf(output, "  cmp eax, 0\n");
            }
            if (is_local_label(ir_temp->args2))
                fprintf(output, "  jne .%s\n", ir_temp->args2);
            else
                fprintf(output, "  jne %s\n", ir_temp->args2);
            break;

        case IR_JMP:
            if (is_local_label(ir_temp->args1))
                fprintf(output, "  jmp .%s\n", ir_temp->args1);
            else
                fprintf(output, "  jmp %s\n", ir_temp->args1);
            break;

        case IR_FORMAT:
        {
            has_format = true;
            has_memory_lib = true;
            int num = ir_temp->call_func_args_num;
            if (num > 0) {
                for (int i = num - 1; i >= 0; i--) {
                    DataType t = ir_temp->call_func_args_type[i];
                    int code = (t == DATA_FLOAT || t == DATA_UFLOAT) ? 1 :
                               (t == DATA_CHAR) ? 2 : (t == DATA_STRING) ? 3 :
                               (t == DATA_BOOL) ? 4 : 0;
                    fprintf(output, "  push %d\n", code);
                }
                fprintf(output, "  mov ebx, esp\n");
                for (int i = num - 1; i >= 0; i--) {
                    char* name = ir_temp->label[i];
                    if (name[0] == '.') {
                        int off = (atoi(name + 2) + 1) * 4;
                        fprintf(output, "  lea eax, [ebp-%d]\n", off);
                        fprintf(output, "  push eax\n");
                    } else {
                        int idx = get_const(name);
                        if (idx == -1) {
                            FATAL(ERR_CG_UNKNOWN_VAR, LOC_NONE, "variable '%s' not found", name);
                        }
                        switch (consts->storage[idx]) {
                        case STORAGE_GLOBAL:
                            fprintf(output, "  push %s\n", name); break;
                        case STORAGE_PARAM:
                            fprintf(output, "  lea eax, [ebp+%d]\n", consts->offset[idx]);
                            fprintf(output, "  push eax\n");
                            break;
                        case STORAGE_LOCAL:
                            fprintf(output, "  lea eax, [ebp-%d]\n", -consts->offset[idx]);
                            fprintf(output, "  push eax\n");
                            break;
                        }
                    }
                }
                fprintf(output, "  push ebx\n");
            } else {
                fprintf(output, "  push 0\n");
            }
            fprintf(output, "  push %d\n", num);
            char* fmt_name = ir_temp->args1;
            if (fmt_name[0] == '.') {
                int off = (atoi(fmt_name + 2) + 1) * 4;
                fprintf(output, "  push dword [ebp-%d]\n", off);
            } else {
                int idx = get_const(fmt_name);
                if (idx != -1) {
                    switch (consts->storage[idx]) {
                    case STORAGE_GLOBAL:
                        fprintf(output, "  push dword [%s]\n", fmt_name); break;
                    case STORAGE_PARAM:
                        fprintf(output, "  push dword [ebp+%d]\n", consts->offset[idx]); break;
                    case STORAGE_LOCAL:
                        fprintf(output, "  push dword [ebp-%d]\n", -consts->offset[idx]); break;
                    }
                } else {
                    if (!is_temp(fmt_name) && get_const((char*)fmt_name) == -1 && !is_number(fmt_name)) {
                        char* label = add_const(DATA_STRING, (char*)fmt_name, 0, DATA_STRING);
                        fprintf(output, "  push %s\n", label);
                    } else {
                        fprintf(output, "  push %s\n", fmt_name);
                    }
                }
            }
            fprintf(output, "  call _format_string\n");
            fprintf(output, "  add esp, %d\n", (num * 2 + 3) * 4);
            cg_store(output, ir_temp->result, DATA_STRING, false);
            break;
        }

        case IR_CALL:
            if (strcmp(ir_temp->args1, "_format_string") == 0)
            {
                has_format = true;
                has_memory_lib = true;
            }
            if (strcmp(ir_temp->args1, "_str_length") == 0 ||
                    strcmp(ir_temp->args1, "_str_char_at") == 0 ||
                    strcmp(ir_temp->args1, "_str_substring") == 0 ||
                    strcmp(ir_temp->args1, "_str_plus_char") == 0 ||
                    strcmp(ir_temp->args1, "_str_concat") == 0 ||
                    strcmp(ir_temp->args1, "_str_equal") == 0 ||
                    strcmp(ir_temp->args1, "_str_not_equal") == 0 ||
                    strcmp(ir_temp->args1, "_char_to_str") == 0 ||
                    strcmp(ir_temp->args1, "_char_concat") == 0 ||
                    strcmp(ir_temp->args1, "_char_pair_to_str") == 0 ||
                    strcmp(ir_temp->args1, "_str_plus_char") == 0 ||
                    strcmp(ir_temp->args1, "_char_plus_str") == 0 ||
                    strcmp(ir_temp->args1, "_str_substring") == 0 ||
                    strcmp(ir_temp->args1, "_str_plus_char2") == 0 ||
                    strcmp(ir_temp->args1, "_str_concat") == 0) {
                has_string_lib = true;
                has_memory_lib = true;
            }

            if (strcmp(ir_temp->args1, "_GetCommandLineArgc") == 0 ||
                    strcmp(ir_temp->args1, "_GetCommandLineArgv") == 0 ||
                    strcmp(ir_temp->args1, "_system_exit") == 0 ||
                    strcmp(ir_temp->args1, "_system_sleep") == 0 ||
                    strcmp(ir_temp->args1, "_system_time") == 0 ||
                    strcmp(ir_temp->args1, "_system_getenv") == 0 ||
                    strcmp(ir_temp->args1, "_system_call") == 0 ||
                    strcmp(ir_temp->args1, "_sys_argv_at") == 0) {
                has_system_lib = true;
                has_memory_lib = true;
                }

            if (strcmp(ir_temp->args1, "_AllocMemory") == 0 ||
                strcmp(ir_temp->args1, "_FreeMemory") == 0) {
                has_memory_lib = true;
                }
            if (strcmp(ir_temp->args1, "_fgt") == 0 ||
                strcmp(ir_temp->args1, "_fge") == 0 ||
                strcmp(ir_temp->args1, "_feq") == 0 ||
                strcmp(ir_temp->args1, "_fne") == 0 ||
                strcmp(ir_temp->args1, "_fle") == 0 ||
                strcmp(ir_temp->args1, "_flt") == 0 ||
                strcmp(ir_temp->args1, "_fmod_fpu") == 0) {
                has_float_lib = true;
                }
            if (strcmp(ir_temp->args1, "_array_to_string") == 0) {
                has_array_lib = true;
            }
            if (strcmp(ir_temp->args1, "_print") == 0 ||
                strcmp(ir_temp->args1, "_set_float_mode") == 0 ||
                strcmp(ir_temp->args1, "_set_float_precision") == 0) {
                has_print = true;
                }
            if (strcmp(ir_temp->args1, "_format_string") == 0) {
                has_format = true;
                has_memory_lib = true;
            }
            if (strcmp(ir_temp->args1, "_input_parse") == 0) {
                has_input = true;
                has_memory_lib = true;
            }
            if (strcmp(ir_temp->args1, "_array_bounds_error") == 0) {
                has_bounds_check = true;
            }
            if (strcmp(ir_temp->args1, "_AllocMemory") == 0) {
                fprintf(output, "  call _gc_alloc\n");
                has_gc = true;
                has_memory_lib = true;
            } else {
                fprintf(output, "  call %s\n", ir_temp->args1);
            }
            if (ir_temp->call_func_args_num > 0) {
                fprintf(output, "  add esp, %d\n", ir_temp->call_func_args_num * 4);
            }
            if (ir_temp->result) {
                bool is_float_call = (ir_temp->data_type == DATA_FLOAT || ir_temp->data_type == DATA_UFLOAT);
                cg_store(output, ir_temp->result, ir_temp->data_type, is_float_call);
                temp_map_add(ir_temp->result, ir_temp->data_type);
            }
            break;

        case IR_LABEL:
            const char* label_name = ir_temp->args1;
            if (main_has_args && strcmp(label_name, "_main") == 0) {
                fprintf(output, "_usermain:\n");
            } else if (is_local_label(ir_temp->args1)) {
                fprintf(output, ".%s:\n", ir_temp->args1);
            } else {
                fprintf(output, "%s:\n", label_name);
            }
            if (ir_temp->has_return)
            {
                temp_map.count = 0;
                IR* scan = ir_temp;
                while (scan != NULL)
                {
                    if (scan != ir_temp && scan->type == IR_LABEL && scan->has_return)
                    {
                        break;
                    }
                    if (scan->result && is_temp(scan->result))
                    {
                        temp_map_add(scan->result, scan->data_type);
                    }
                    scan = scan->next;
                }

                current_function = ir_temp->args1;
                current_param_offset = 8;
                current_param_index = 0;
                in_function = true;

                int max_temp_idx = -1;
                int local_var_count = 0;
                int total_array_bytes = 0;
                scan = ir_temp->next;
                while (scan) {
                    if (scan->type == IR_LABEL && scan->has_return) break;

                    if (scan->result && scan->result[0] == '.'
                        && scan->result[1] == 't'
                        && isdigit((unsigned char)scan->result[2])) {
                        int idx = atoi(scan->result + 2);
                        if (idx > max_temp_idx) max_temp_idx = idx;
                    }

                    if (scan->args1 && scan->args1[0] == '.'
                        && scan->args1[1] == 't'
                        && isdigit((unsigned char)scan->args1[2])) {
                        int idx = atoi(scan->args1 + 2);
                        if (idx > max_temp_idx) max_temp_idx = idx;
                    }

                    if (scan->args2 && scan->args2[0] == '.'
                        && scan->args2[1] == 't'
                        && isdigit((unsigned char)scan->args2[2])) {
                        int idx = atoi(scan->args2 + 2);
                        if (idx > max_temp_idx) max_temp_idx = idx;
                    }

                    if (scan->type == IR_VAR || scan->type == IR_VAR_UNINIT)
                        local_var_count++;
                    if (scan->result && strncmp(scan->result, "_foreach_idx", 12) == 0)
                        local_var_count++;

                    if (scan->type == IR_ARRAY_DECL)
                        total_array_bytes += scan->array_size * scan->elem_size;
                    scan = scan->next;
                }
                int func_temp_count = ir_temp->func_temp_count;
                if (func_temp_count < max_temp_idx + 1)
                    func_temp_count = max_temp_idx + 1;
                int temp_area_size = (func_temp_count + 16) * 4;
                int local_vars_bytes = local_var_count * 4;
                int stack_size = temp_area_size + local_vars_bytes + total_array_bytes + 8;
                stack_size = (stack_size + 15) & ~15;

                current_local_offset = -(temp_area_size + 8);

                current_func_callee_mask = regmap_callee_saved_mask(g_regmap);
                fprintf(output, "  push ebp\n");
                fprintf(output, "  mov ebp, esp\n");
                {
                    current_func_callee_mask = regmap_callee_saved_mask(g_regmap);
                    if (current_func_callee_mask & CALLEE_SAVED_EBX) fprintf(output, "  push ebx\n");
                    if (current_func_callee_mask & CALLEE_SAVED_ESI) fprintf(output, "  push esi\n");
                    if (current_func_callee_mask & CALLEE_SAVED_EDI) fprintf(output, "  push edi\n");
                }
                fprintf(output, "  sub esp, %d\n", stack_size);

                if (strcmp(ir_temp->args1, "_main") == 0 && !main_has_args) {
                    fprintf(output, "  push 65001\n");
                    fprintf(output, "  call _SetConsoleOutputCP@4\n");
                    fprintf(output, "  finit\n");
                    fprintf(output, "  call _gc_init\n");
                    in_main = true;
                    has_gc = true;
                    has_memory_lib = true;
                }
            }
            break;

        case IR_PARAM_DEF:
            add_symbol(ir_temp->data_type, "", ir_temp->result, TYPE_VAR, ir_temp->data_type, STORAGE_PARAM, current_param_offset,false,0);
            current_param_offset += 4;
            current_param_index++;
            break;

        case IR_RET:
            if (in_main) {
                if (ir_temp->has_return) {
                    char* ret_val = ir_temp->args1;
                    if (ret_val != NULL && is_number(ret_val) && strchr(ret_val, '.') != NULL)
                    {
                        char* label = add_const(DATA_FLOAT, ret_val, 0, DATA_FLOAT);
                        fprintf(output, "  fld dword [%s]\n", label);
                    } else if (is_temp(ret_val)) {
                        int off = (atoi(ret_val + 2) + 1) * 4;
                        fprintf(output, "  mov eax, dword [ebp-%d]\n", off);
                    } else if (get_const(ret_val) != -1) {
                        int idx = get_const(ret_val);
                        char addr[100];
                        switch (consts->storage[idx]) {
                        case STORAGE_GLOBAL: sprintf(addr, "[%s]", ret_val); break;
                        case STORAGE_PARAM: sprintf(addr, "[ebp+%d]", consts->offset[idx]); break;
                        case STORAGE_LOCAL: sprintf(addr, "[ebp-%d]", -consts->offset[idx]); break;
                        }
                        fprintf(output, "  mov eax, dword %s\n", addr);
                    } else {
                        fprintf(output, "  mov eax, %s\n", ret_val);
                    }
                } else {
                    fprintf(output, "  xor eax, eax\n");
                }
                fprintf(output, "  push eax\n");
                fprintf(output, "  call _ExitProcess@4\n");
                in_main = false;
            } else
            {
                if (ir_temp->has_return) {
                    char* ret_val = ir_temp->args1;
                    DataType ret_type = ir_temp->data_type;
                    if (ret_type == DATA_FLOAT || ret_type == DATA_UFLOAT) {
                        if (is_temp(ret_val)) {
                            int off = (atoi(ret_val + 2) + 1) * 4;
                            fprintf(output, "  fld dword [ebp-%d]\n", off);
                        } else {
                            int idx = get_const(ret_val);
                            if (idx != -1) {
                                char addr[100];
                                switch (consts->storage[idx]) {
                                case STORAGE_LOCAL:
                                    sprintf(addr, "[ebp-%d]", -consts->offset[idx]); break;
                                case STORAGE_PARAM:
                                    sprintf(addr, "[ebp+%d]", consts->offset[idx]); break;
                                case STORAGE_GLOBAL:
                                    sprintf(addr, "[%s]", ret_val); break;
                                default: sprintf(addr, "[%s]", ret_val); break;
                                }
                                fprintf(output, "  fld dword %s\n", addr);
                            } else {
                                char* label = add_const(DATA_FLOAT, ret_val, 0, DATA_FLOAT);
                                fprintf(output, "  fld dword [%s]\n", label);
                            }
                        }
                    } else {
                        cg_load(output, ret_val, ret_type);
                    }
                }
                if (current_func_callee_mask & CALLEE_SAVED_EDI) fprintf(output, "  pop edi\n");
                if (current_func_callee_mask & CALLEE_SAVED_ESI) fprintf(output, "  pop esi\n");
                if (current_func_callee_mask & CALLEE_SAVED_EBX) fprintf(output, "  pop ebx\n");
                fprintf(output, "  leave\n");
                fprintf(output, "  ret\n");
                {
                    IR* next = ir_temp->next;

                    while (next != NULL &&
                           next->type == IR_LABEL &&
                           !next->has_return) {
                        next = next->next;
                           }

                    if (next == NULL ||
                        (next->type == IR_LABEL && next->has_return)) {
                        in_function = false;
                        }
                }
            }
            break;

        case IR_ALLOC_CLASS: {
                fprintf(output, "  push %s\n", ir_temp->args1);
                fprintf(output, "  call _gc_alloc\n");
                fprintf(output, "  add esp, 4\n");
                cg_store(output, ir_temp->result, DATA_INT, false);
                has_memory_lib = true;
                has_gc = true;
                break;
        }

        case IR_LOAD_MEMBER: {
                int obj_offset = (atoi(ir_temp->args1 + 2) + 1) * 4;
                int mem_off = atoi(ir_temp->args2);
                fprintf(output, "  mov eax, [ebp-%d]\n", obj_offset);
                if (ir_temp->data_type == DATA_FLOAT || ir_temp->data_type == DATA_UFLOAT) {
                    fprintf(output, "  fld dword [eax+%d]\n", mem_off);
                    cg_store(output, ir_temp->result, ir_temp->data_type, true);
                } else if (ir_temp->data_type == DATA_CHAR || ir_temp->data_type == DATA_BOOL) {
                    fprintf(output, "  movzx eax, byte [eax+%d]\n", mem_off);
                    cg_store(output, ir_temp->result, ir_temp->data_type, false);
                } else {
                    fprintf(output, "  mov eax, [eax+%d]\n", mem_off);
                    cg_store(output, ir_temp->result, ir_temp->data_type, false);
                }
                if (ir_temp->result && is_temp(ir_temp->result)) {
                    temp_map_add(ir_temp->result, ir_temp->data_type);
                }
                break;
        }

        case IR_LOAD_THIS: {
                fprintf(output, "  mov eax, [ebp+8]\n");
                cg_store(output, ir_temp->result, DATA_INT, false);
                if (ir_temp->result && is_temp(ir_temp->result)) {
                    temp_map_add(ir_temp->result, DATA_INT);
                }
                break;
        }

        case IR_BOUNDS_CHECK:
            {
                char* idx = ir_temp->args1;
                char* dim = ir_temp->args2;

                if (is_temp(idx)) {
                    int off = (atoi(idx + 2) + 1) * 4;
                    fprintf(output, "  mov eax, dword [ebp-%d]\n", off);
                } else {
                    int idx_const = get_const(idx);
                    if (idx_const != -1) {
                        char addr[100];
                        switch (consts->storage[idx_const]) {
                        case STORAGE_GLOBAL: sprintf(addr, "[%s]", idx); break;
                        case STORAGE_PARAM:  sprintf(addr, "[ebp+%d]", consts->offset[idx_const]); break;
                        case STORAGE_LOCAL:  sprintf(addr, "[ebp-%d]", -consts->offset[idx_const]); break;
                        }
                        fprintf(output, "  mov eax, dword %s\n", addr);
                    } else {
                        fprintf(output, "  mov eax, %s\n", idx);
                    }
                }

                if (is_temp(dim)) {
                    int off = (atoi(dim + 2) + 1) * 4;
                    fprintf(output, "  mov ebx, dword [ebp-%d]\n", off);
                } else {
                    int dim_const = get_const(dim);
                    if (dim_const != -1) {
                        char addr[100];
                        switch (consts->storage[dim_const]) {
                        case STORAGE_GLOBAL: sprintf(addr, "[%s]", dim); break;
                        case STORAGE_PARAM:  sprintf(addr, "[ebp+%d]", consts->offset[dim_const]); break;
                        case STORAGE_LOCAL:  sprintf(addr, "[ebp-%d]", -consts->offset[dim_const]); break;
                        }
                        fprintf(output, "  mov ebx, dword %s\n", addr);
                    } else {
                        fprintf(output, "  mov ebx, %s\n", dim);
                    }
                }

                fprintf(output, "  cmp eax, ebx\n");
                fprintf(output, "  jge _array_bounds_error\n");
                fprintf(output, "  cmp eax, 0\n");
                fprintf(output, "  jl _array_bounds_error\n");

                has_bounds_check = true;
                break;
            }
        case IR_COPY_BLOCK:
            {
                char* dst_ptr = ir_temp->args1;
                char* src_ptr = ir_temp->args2;
                int byte_count = atoi(ir_temp->result);

                cg_load(output, dst_ptr, DATA_INT);
                fprintf(output, "  mov edi, eax\n");

                cg_load(output, src_ptr, DATA_INT);
                fprintf(output, "  mov esi, eax\n");

                int dwords = (byte_count + 3) / 4;
                if (dwords > 0)
                {
                    fprintf(output, "  mov ecx, %d\n", dwords);
                    fprintf(output, "  cld\n");
                    fprintf(output, "  rep movsd\n");
                }
                int remainder = byte_count % 4;
                if (remainder > 0)
                {
                    fprintf(output, "  mov ecx, %d\n", remainder);
                    fprintf(output, "  rep movsb\n");
                }
                break;
            }
        case IR_PHI:
            if (ir_temp->result) {
                if (is_temp(ir_temp->result)) {
                    temp_map_add(ir_temp->result, ir_temp->data_type);
                } else {
                    const char* base = strip_ssa_suffix(ir_temp->result);
                    if (get_const(base) == -1) {
                        Manual_add_const(ir_temp->data_type, "", 0, ir_temp->result,
                                         TYPE_VAR, ir_temp->data_type);
                    }
                }
            }
        }
        ir_temp = ir_temp->next;
    }
}

void codegen_win32_end(FILE* output)
{
    fprintf(output,"section .text\n");
    fprintf(output,"global _main\n");
    fprintf(output,"extern _ExitProcess@4\n");
    fprintf(output, "extern _SetConsoleOutputCP@4\n");
    if (has_string_lib)
    {
        fprintf(output,"extern _str_length\n");
        fprintf(output,"extern _str_copy\n");
        fprintf(output,"extern _str_concat\n");
        fprintf(output,"extern _char_to_str\n");
        fprintf(output,"extern _char_concat\n");
        fprintf(output,"extern _char_pair_to_str\n");
        fprintf(output,"extern _str_plus_char\n");
        fprintf(output,"extern _char_plus_str\n");
        fprintf(output,"extern _str_equal\n");
        fprintf(output,"extern _str_not_equal\n");
        fprintf(output,"extern _str_char_at\n");
        fprintf(output,"extern _str_substring\n");
        fprintf(output,"extern _str_plus_char_2\n");
        fprintf(output,"extern _char_plus_str_2\n");
    }
    if (main_has_args)
    {
        fprintf(output, "extern _GetCommandLineArgc\n");
        fprintf(output, "extern _GetCommandLineArgv\n");
        has_memory_lib = true;
    }
    if (has_memory_lib)
    {
        fprintf(output,"extern _AllocMemory\n");
        fprintf(output,"extern _FreeMemory\n");
        has_print = true;
    }
    if (has_print)
    {
        fprintf(output,"extern _print\n");
        fprintf(output, "extern _set_float_mode\n");
        fprintf(output, "extern _set_float_precision\n");
    }
    if (has_float_lib)
    {
        fprintf(output,"extern _fgt\n");
        fprintf(output,"extern _fge\n");
        fprintf(output,"extern _feq\n");
        fprintf(output,"extern _fne\n");
        fprintf(output,"extern _fle\n");
        fprintf(output,"extern _flt\n");
        fprintf(output,"extern _fmod_fpu\n");
    }
    if (has_bounds_check) {
        fprintf(output, "extern _array_bounds_error\n");
    }
    if (has_array_lib)
    {
        fprintf(output,"extern _array_to_string\n");
    }
    if (has_format)
    {
        fprintf(output, "extern _format_string\n");
    }
    if (has_input)
    {
        fprintf(output, "extern _input_parse\n");
    }

    if (has_system_lib || main_has_args) {
        fprintf(output, "extern _GetCommandLineArgc\n");
        fprintf(output, "extern _GetCommandLineArgv\n");
        fprintf(output, "extern _system_exit\n");
        fprintf(output, "extern _system_sleep\n");
        fprintf(output, "extern _system_time\n");
        fprintf(output, "extern _system_getenv\n");
        fprintf(output, "extern _system_call\n");
        fprintf(output, "extern _sys_argv_at\n");
    }

    if (main_has_args) {
        fprintf(output, "_main:\n");
        fprintf(output, "  finit\n");
        fprintf(output, "  push ebp\n");
        fprintf(output, "  mov ebp, esp\n");
        fprintf(output, "  call _GetCommandLineArgc\n");
        fprintf(output, "  mov ebx, eax\n");
        fprintf(output, "  call _GetCommandLineArgv\n");
        fprintf(output, "  mov esi, eax\n");
        fprintf(output, "  mov eax, ebx\n");
        fprintf(output, "  shl eax, 2\n");
        fprintf(output, "  add eax, 4\n");
        fprintf(output, "  push ebx\n");
        fprintf(output, "  push eax\n");
        fprintf(output, "  call _gc_alloc\n");
        fprintf(output, "  add esp, 4\n");
        fprintf(output, "  pop ebx\n");
        fprintf(output, "  mov [eax], ebx\n");
        fprintf(output, "  add eax, 4\n");
        fprintf(output, "  xor ecx, ecx\n");
        fprintf(output, "_copy_argv:\n");
        fprintf(output, "  cmp ecx, ebx\n");
        fprintf(output, "  jge _call_usermain\n");
        fprintf(output, "  mov edx, [esi + ecx*4]\n");
        fprintf(output, "  mov [eax + ecx*4], edx\n");
        fprintf(output, "  inc ecx\n");
        fprintf(output, "  jmp _copy_argv\n");
        fprintf(output, "_call_usermain:\n");
        fprintf(output, "  push eax\n");
        fprintf(output, "  call _usermain\n");
        fprintf(output, "  add esp, 4\n");
        fprintf(output, "  push eax\n");
        fprintf(output, "  call _ExitProcess@4\n");

        has_memory_lib = true;
        has_gc = true;
    }

    if (has_gc) {
        fprintf(output, "extern _gc_init\n");
        fprintf(output, "extern _gc_alloc\n");
        fprintf(output, "extern _gc_collect\n");
        fprintf(output, "extern _gc_force\n");
        fprintf(output, "extern _gc_status\n");
    }
    if (has_gc) {
        fprintf(output, "global gc_data_start_label\n");
        fprintf(output, "global gc_data_end_label\n");
        fprintf(output, "global gc_bss_start_label\n");
        fprintf(output, "global gc_bss_end_label\n");
    }
    fprintf(output,"section .data\n");
    if (has_gc) {
        fprintf(output, "  gc_data_start_label:\n");
    }
    for (int i = 0;i < consts->index;i++)
    {
        if (consts->var_type[i] == TYPE_VAR && consts->storage[i] == STORAGE_GLOBAL)
        {
            switch (consts->type[i])
            {
            case DATA_INT:
                if (is_var_name(consts->value[i]))
                {
                    fprintf(output,"  %s dd 0\n",consts->name[i]);
                } else
                {
                    fprintf(output,"  %s dd %s\n",consts->name[i],consts->value[i]);
                }
                break;
            case DATA_FLOAT:
                if (is_var_name(consts->value[i]))
                {
                    fprintf(output,"  %s dd 0.0\n",consts->name[i]);
                } else
                {
                    const char* val = consts->value[i];
                    if (val && val[0] == '.') {
                        fprintf(output,"  %s dd 0%s\n", consts->name[i], val);
                    } else if (val && val[strlen(val)-1] == '.') {
                        fprintf(output,"  %s dd %s0\n", consts->name[i], val);
                    } else {
                        fprintf(output,"  %s dd %s\n", consts->name[i], val);
                    }
                }
                break;
            case DATA_BOOL:
                if (is_var_name(consts->value[i]))
                {
                    fprintf(output,"  %s db 0,0\n",consts->name[i]);
                } else
                {
                    fprintf(output,"  %s db %s,0\n",consts->name[i],consts->value[i]);
                }
                break;
            case DATA_CHAR:
                if (is_var_name(consts->value[i]) && consts->value_type[i] != DATA_CHAR)
                {
                    fprintf(output,"  %s db ` `,0\n",consts->name[i]);
                } else
                {
                    char* esc = escape_for_nasm(consts->value[i]);
                    fprintf(output,"  %s db `%s`,0\n",consts->name[i], esc);
                    free(esc);
                }
                break;
            case DATA_STRING:
                if (is_var_name(consts->value[i]) && consts->value_type[i] != DATA_STRING)
                {
                    fprintf(output,"  %s db ` `,0\n",consts->name[i]);
                } else
                {
                    char* esc = escape_for_nasm(consts->value[i]);
                    fprintf(output,"  %s db `%s`,0\n",consts->name[i], esc);
                    free(esc);
                }
                break;
            }
        }
    }

    if (has_gc) {
        fprintf(output, "  gc_data_end_label:\n");
    }

    fprintf(output,"section .bss\n");
    if (has_gc) {
        fprintf(output, "  gc_bss_start_label:\n");
    }
    for (int i = 0;i < consts->index;i++)
    {
        if (consts->var_type[i] == TYPE_UNINIT_VAR && consts->storage[i] == STORAGE_GLOBAL)
        {
            switch (consts->type[i])
            {
            case DATA_INT:
                fprintf(output,"  %s resd 1\n",consts->name[i]);
                break;
            case DATA_FLOAT:
                fprintf(output,"  %s resd 2\n",consts->name[i]);
                break;
            case DATA_BOOL:
                fprintf(output,"  %s resb 4\n",consts->name[i]);
                break;
            case DATA_CHAR:
                fprintf(output,"  %s resb 1\n",consts->name[i]);
                break;
            case DATA_STRING:
                fprintf(output,"  %s resb 4\n",consts->name[i]);
                break;
            }
        }
    }

    if (has_gc) {
        fprintf(output, "  gc_bss_end_label:\n");
    }

    fprintf(output,"section .rodata\n");
    for (int i = 0;i < consts->index;i++)
    {
        if (consts->var_type[i] == TYPE_CONST)
        {
            switch (consts->type[i])
            {
            case DATA_INT:
                if (is_var_name(consts->value[i]))
                {
                    fprintf(output,"  %s dd 0\n",consts->name[i]);
                } else
                {
                    fprintf(output,"  %s dd %s\n",consts->name[i],consts->value[i]);
                }
                break;
            case DATA_FLOAT:
                if (is_var_name(consts->value[i]))
                {
                    fprintf(output,"  %s dd 0.0\n",consts->name[i]);
                } else
                {
                    const char* val = consts->value[i];
                    if (val && val[0] == '.') {
                        fprintf(output,"  %s dd 0%s\n", consts->name[i], val);
                    } else if (val && val[strlen(val)-1] == '.') {
                        fprintf(output,"  %s dd %s0\n", consts->name[i], val);
                    } else {
                        fprintf(output,"  %s dd %s\n", consts->name[i], val);
                    }
                }
                break;
            case DATA_BOOL:
                if (is_var_name(consts->value[i]))
                {
                    fprintf(output,"  %s db `false`,0\n",consts->name[i]);
                } else
                {
                    char* esc = escape_for_nasm(consts->value[i]);
                    fprintf(output,"  %s db `%s`,0\n",consts->name[i], esc);
                    free(esc);
                }
                break;
            case DATA_CHAR:
                if (is_var_name(consts->value[i]) && consts->value_type[i] != DATA_CHAR)
                {
                    fprintf(output,"  %s db ` `,0\n",consts->name[i]);
                } else
                {
                    char* esc = escape_for_nasm(consts->value[i]);
                    fprintf(output,"  %s db `%s`,0\n",consts->name[i], esc);
                    free(esc);
                }
                break;
            case DATA_STRING:
                if (is_var_name(consts->value[i]) && consts->value_type[i] != DATA_STRING)
                {
                    fprintf(output,"  %s db ` `,0\n",consts->name[i]);
                } else
                {
                    char* esc = escape_for_nasm(consts->value[i]);
                    fprintf(output,"  %s db `%s`,0\n",consts->name[i], esc);
                    free(esc);
                }
                break;
            }
        }
    }
}

void cleanup_consts()
{
    if (consts != NULL) {
        for (int i = 0; i < consts->index; i++) {
            free(consts->value[i]);
            free(consts->name[i]);
        }
        free(consts->type);
        free(consts->value);
        free(consts->name);
        free(consts->var_type);
        free(consts->value_type);
        free(consts->storage);
        free(consts->offset);
        free(consts->is_array);
        free(consts->array_size);
        free(consts);
        consts = NULL;
    }
    temp_map_cleanup();
}

void codegen_cleanup(void)
{
    cleanup_consts();
    temp_map_cleanup();
}