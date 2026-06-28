#ifndef REGMAP_H
#define REGMAP_H

#include <stdbool.h>

typedef enum Reg {
    R_NONE = -1,
    R_EAX  = 0,
    R_ECX  = 1,
    R_EDX  = 2,
    R_EBX  = 3,
    R_ESI  = 4,
    R_EDI  = 5,
    R_MAX  = 6
} Reg;

#define CALLEE_SAVED_EBX  (1u << R_EBX)
#define CALLEE_SAVED_ESI  (1u << R_ESI)
#define CALLEE_SAVED_EDI  (1u << R_EDI)

#define ALL_GP_REGS \
((1u << R_EAX) | (1u << R_ECX) | (1u << R_EDX) | \
(1u << R_EBX) | (1u << R_ESI) | (1u << R_EDI))

typedef struct RegMapEntry {
    Reg   reg;
    bool  is_float;
    int   spill_slot;
} RegMapEntry;

typedef struct RegMap {
    char**        names;
    RegMapEntry*  entries;
    int           count;
    int           capacity;
    unsigned      callee_saved_mask;
} RegMap;

extern RegMap* g_regmap;

void        regmap_init(void);
void        regmap_cleanup(void);
void        regmap_add(RegMap* map, const char* name, Reg reg,
                       bool is_float, int spill_slot);
const RegMapEntry* regmap_get(const RegMap* map, const char* name);
unsigned    regmap_callee_saved_mask(const RegMap* map);

void        regmap_allocate_from_cfg(void* cfg, int num_regs);

#endif