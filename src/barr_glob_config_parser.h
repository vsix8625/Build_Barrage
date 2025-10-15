#pragma GCC system_header
#ifndef BARR_GLOB_CONFIG_PARSER_H
#define BARR_GLOB_CONFIG_PARSER_H

#include "barr_defs.h"

typedef enum
{
    TYPE_UNKNOWN = 0,

    TYPE_STR,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,

    COUNT_TYPES
} BARR_ConfigValueType;

typedef struct BARR_ConfigEntry
{
    char *key;
    BARR_ConfigValueType type;
    union
    {
        char *str_val;
        barr_i32 int_val;
        barr_f32 float_val;
        bool bool_val;
    } value;
} BARR_ConfigEntry;

typedef struct BARR_ConfigTable BARR_ConfigTable;
#define BARR_CONFIG_MAX_ENTRIES 128
#define BARR_CONFIG_ARENA_SIZE (16 * BARR_BUF_SIZE_1024)

typedef struct BARR_ConfigTable
{
    BARR_ConfigEntry *entries;
    size_t count;
    size_t capacity;

    void (*destroy)(BARR_ConfigTable *);
} BARR_ConfigTable;

void BARR_config_table_add(BARR_ConfigTable *t, const BARR_ConfigEntry *src);
BARR_ConfigTable *BARR_config_parse_file(const char *filename);
BARR_ConfigEntry *BARR_config_table_get(BARR_ConfigTable *t, const char *key);

#endif  // BARR_GLOB_CONFIG_PARSER_H
