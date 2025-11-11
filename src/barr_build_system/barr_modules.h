#ifndef BARR_MODULES_H_
#define BARR_MODULES_H_

#include "barr_defs.h"

typedef struct
{
    char *name;
    char *path;
} BARR_Module;

// getters
BARR_Module *BARR_get_module_array(void);
size_t BARR_get_module_count(void);

bool BARR_add_module(const char *name, const char *path, const char *required);
BARR_Module *BARR_get_module(const char *name);
void BARR_print_modules(void);

#endif  // BARR_MODULES_H_
