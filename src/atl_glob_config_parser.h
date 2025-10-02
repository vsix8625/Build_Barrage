#pragma GCC system_header
#ifndef ATL_GLOB_CONFIG_PARSER_H
#define ATL_GLOB_CONFIG_PARSER_H

#include "atl_defs.h"

typedef enum
{
    TYPE_UNKNOWN = 0,

    TYPE_STR,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,

    COUNT_TYPES
} ATL_ConfigValueType;

typedef struct ATL_ConfigEntry
{
    char *key;
    ATL_ConfigValueType type;
    union
    {
        char *str_val;
        atl_i32 int_val;
        atl_f32 float_val;
        bool bool_val;
    } value;
} ATL_ConfigEntry;

typedef struct ATL_ConfigTable ATL_ConfigTable;
#define ATL_CONFIG_MAX_ENTRIES 128
#define ATL_CONFIG_ARENA_SIZE (16 * ATL_BUF_SIZE_1024)

typedef struct ATL_ConfigTable
{
    ATL_ConfigEntry *entries;
    size_t count;
    size_t capacity;

    void (*destroy)(ATL_ConfigTable *);
} ATL_ConfigTable;

void ATL_config_table_add(ATL_ConfigTable *t, const ATL_ConfigEntry *src);
ATL_ConfigTable *ATL_config_parse_file(const char *filename);
ATL_ConfigEntry *ATL_config_table_get(ATL_ConfigTable *t, const char *key);

#endif  // ATL_GLOB_CONFIG_PARSER_H
