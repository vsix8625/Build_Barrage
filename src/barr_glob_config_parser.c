#include "barr_glob_config_parser.h"
#include "barr_arena.h"
#include "barr_io.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//----------------------------------------------------------------------------------------------------

void barr_destroy_table(BARR_ConfigTable *t);

//----------------------------------------------------------------------------------------------------

static BARR_Arena g_barr_config_arena;

BARR_ConfigTable *barr_config_table_init(void)
{
    BARR_arena_init(&g_barr_config_arena, BARR_CONFIG_ARENA_SIZE, "config_parser_arena", 8);

    BARR_ConfigTable *t = BARR_arena_alloc(&g_barr_config_arena, sizeof(BARR_ConfigTable));
    if (!t)
    {
        BARR_errlog("Failed to allocate memory for BARR_ConfigTable: %s:%d", __func__, __LINE__);
        return NULL;
    }

    t->entries = BARR_arena_alloc(&g_barr_config_arena, sizeof(BARR_ConfigEntry) * BARR_CONFIG_MAX_ENTRIES);
    if (!t->entries)
    {
        BARR_errlog("Failed to allocate memory for BARR_ConfigEntry: %s:%d", __func__, __LINE__);
        return NULL;
    }

    t->count = 0;
    t->capacity = BARR_CONFIG_MAX_ENTRIES;
    t->destroy = barr_destroy_table;

    return t;
}

//----------------------------------------------------------------------------------------------------

BARR_ConfigValueType barr_config_parse_type(const char *type)
{
    if (BARR_strmatch(type, "string") || BARR_strmatch(type, "str"))
    {
        return TYPE_STR;
    }
    if (BARR_strmatch(type, "int"))
    {
        return TYPE_INT;
    }
    if (BARR_strmatch(type, "float"))
    {
        return TYPE_FLOAT;
    }
    if (BARR_strmatch(type, "bool"))
    {
        return TYPE_BOOL;
    }
    return TYPE_UNKNOWN;
}

//----------------------------------------------------------------------------------------------------

#define BARR_CONFIG_MAX_LINE_PARSE BARR_BUF_SIZE_1024

void barr_config_parse_line(const char *line, BARR_ConfigTable *t)
{
    char buffer[BARR_CONFIG_MAX_LINE_PARSE];

    BARR_safecpy(buffer, line, sizeof(buffer) - 1);

    char *semicolon = strchr(buffer, ';');
    if (!semicolon)
    {
        return;
    }
    *semicolon = '\0';

    char *colon = strchr(buffer, ':');
    if (!colon)
    {
        return;
    }
    *colon = '\0';

    char *equal = strchr(colon + 1, '=');
    if (!equal)
    {
        return;
    }
    *equal = '\0';

    char *key = buffer;
    char *type = colon + 1;
    char *value = equal + 1;

    BARR_trim(key);
    BARR_trim(type);
    BARR_trim(value);

    BARR_ConfigValueType val_type = barr_config_parse_type(type);

    if (val_type == TYPE_STR)
    {
        size_t len = strlen(value);
        if (len < 2 || value[0] != '"' || value[len - 1] != '"')
        {
            BARR_errlog("Type string values must be in quotes");
            BARR_errlog("In %s key: value", key);
            return;
        }

        value[len - 1] = '\0';
        value = value + 1;
    }

    if (val_type == TYPE_UNKNOWN)
    {
        BARR_errlog("Unknown type for key %s", key);
        return;
    }

    BARR_ConfigEntry entry = {0};
    entry.key = BARR_arena_strdup(&g_barr_config_arena, key);
    entry.type = val_type;

    switch (val_type)
    {
        case TYPE_STR:
        {
            entry.value.str_val = BARR_arena_strdup(&g_barr_config_arena, value);
        }
        break;
        case TYPE_INT:
        {
            entry.value.int_val = atoi(value);
        }
        break;
        case TYPE_FLOAT:
        {
            entry.value.float_val = strtof(value, NULL);
        }
        break;
        case TYPE_BOOL:
        {
            entry.value.bool_val = BARR_strmatch(value, "true") || atoi(value) != 0;
        }
        break;
        default:
        {
        }
        break;
    }

    BARR_config_table_add(t, &entry);
}

void BARR_config_table_add(BARR_ConfigTable *t, const BARR_ConfigEntry *src)
{
    if (t->count >= t->capacity)
    {
        BARR_errlog("BARR_ConfigTable capacity exceeded!");
        return;
    }

    BARR_ConfigEntry *entry = &t->entries[t->count++];

    entry->type = src->type;
    entry->key = BARR_arena_strdup(&g_barr_config_arena, src->key);

    switch (src->type)
    {
        case TYPE_STR:
            entry->value.str_val = BARR_arena_strdup(&g_barr_config_arena, src->value.str_val);
            break;
        case TYPE_INT:
            entry->value.int_val = src->value.int_val;
            break;
        case TYPE_FLOAT:
            entry->value.float_val = src->value.float_val;
            break;
        case TYPE_BOOL:
            entry->value.bool_val = src->value.bool_val;
            break;
        default:
            break;
    }
}

//----------------------------------------------------------------------------------------------------

BARR_ConfigEntry *BARR_config_table_get(BARR_ConfigTable *t, const char *key)
{
    if (!t)
    {
        BARR_errlog("%s: Config table is NULL", __func__);
        return NULL;
    }

    for (size_t i = 0; i < t->count; i++)
    {
        if (BARR_strmatch(t->entries[i].key, key))
        {
            return &t->entries[i];
        }
    }

    BARR_errlog("Key not found: %s", key);
    return NULL;
}

//----------------------------------------------------------------------------------------------------

BARR_ConfigTable *BARR_config_parse_file(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        BARR_errlog("Failed to open %s file to read", filename);
        BARR_warnlog("Does the file exists?");
        return NULL;
    }

    BARR_ConfigTable *t = barr_config_table_init();
    if (!t)
    {
        BARR_errlog("Failed to initialize BARR_ConfigTable");
        fclose(f);
        return NULL;
    }

    char line[BARR_CONFIG_MAX_LINE_PARSE];

    while (fgets(line, sizeof(line), f))
    {
        char *ptr = line;

        while (isspace((barr_u8) *ptr))
        {
            ptr++;
        }

        if (*ptr == '\0' || *ptr == '#' || *ptr == '\n')
        {
            continue;
        }

        barr_config_parse_line(ptr, t);
    }
    fclose(f);
    return t;
}

//----------------------------------------------------------------------------------------------------

void barr_destroy_table(BARR_ConfigTable *t)
{
    if (t)
    {
        BARR_destroy_arena(&g_barr_config_arena);
    }
}
