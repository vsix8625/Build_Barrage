#include "atl_glob_config_parser.h"
#include "atl_arena.h"
#include "atl_io.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//----------------------------------------------------------------------------------------------------

void atl_trim(char *s);
void atl_destroy_table(ATL_ConfigTable *t);

//----------------------------------------------------------------------------------------------------

static ATL_Arena g_atl_config_arena;

ATL_ConfigTable *atl_config_table_init(void)
{
    ATL_arena_init(&g_atl_config_arena, ATL_CONFIG_ARENA_SIZE, "config_parser_arena");

    ATL_ConfigTable *t = ATL_arena_alloc(&g_atl_config_arena, sizeof(ATL_ConfigTable));
    if (!t)
    {
        ATL_errlog("Failed to allocate memory for ATL_ConfigTable: %s:%d", __func__, __LINE__);
        return NULL;
    }

    t->entries = ATL_arena_alloc(&g_atl_config_arena, sizeof(ATL_ConfigEntry) * ATL_CONFIG_MAX_ENTRIES);
    if (!t->entries)
    {
        ATL_errlog("Failed to allocate memory for ATL_ConfigEntry: %s:%d", __func__, __LINE__);
        return NULL;
    }

    t->count = 0;
    t->capacity = ATL_CONFIG_MAX_ENTRIES;
    t->destroy = atl_destroy_table;

    return t;
}

//----------------------------------------------------------------------------------------------------

ATL_ConfigValueType atl_config_parse_type(const char *type)
{
    if (ATL_strmatch(type, "string") || ATL_strmatch(type, "str"))
    {
        return TYPE_STR;
    }
    if (ATL_strmatch(type, "int"))
    {
        return TYPE_INT;
    }
    if (ATL_strmatch(type, "float"))
    {
        return TYPE_FLOAT;
    }
    if (ATL_strmatch(type, "bool"))
    {
        return TYPE_BOOL;
    }
    return TYPE_UNKNOWN;
}

//----------------------------------------------------------------------------------------------------

#define ATL_CONFIG_MAX_LINE_PARSE ATL_BUF_SIZE_1024

void atl_config_parse_line(const char *line, ATL_ConfigTable *t)
{
    char buffer[ATL_CONFIG_MAX_LINE_PARSE];

    ATL_safecpy(buffer, line, sizeof(buffer) - 1);

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

    atl_trim(key);
    atl_trim(type);
    atl_trim(value);

    ATL_ConfigValueType val_type = atl_config_parse_type(type);

    if (val_type == TYPE_STR)
    {
        size_t len = strlen(value);
        if (len < 2 || value[0] != '"' || value[len - 1] != '"')
        {
            ATL_errlog("Type string values must be in quotes");
            ATL_errlog("In %s key: value", key);
            return;
        }

        value[len - 1] = '\0';
        value = value + 1;
    }

    if (val_type == TYPE_UNKNOWN)
    {
        ATL_errlog("Unknown type for key %s", key);
        return;
    }

    ATL_ConfigEntry entry = {0};
    entry.key = ATL_arena_strdup(&g_atl_config_arena, key);
    entry.type = val_type;

    switch (val_type)
    {
        case TYPE_STR:
        {
            entry.value.str_val = ATL_arena_strdup(&g_atl_config_arena, value);
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
            entry.value.bool_val = ATL_strmatch(value, "true") || atoi(value) != 0;
        }
        break;
        default:
        {
        }
        break;
    }

    ATL_config_table_add(t, &entry);
}

void ATL_config_table_add(ATL_ConfigTable *t, const ATL_ConfigEntry *src)
{
    if (t->count >= t->capacity)
    {
        ATL_errlog("ATL_ConfigTable capacity exceeded!");
        return;
    }

    ATL_ConfigEntry *entry = &t->entries[t->count++];

    entry->type = src->type;
    entry->key = ATL_arena_strdup(&g_atl_config_arena, src->key);

    switch (src->type)
    {
        case TYPE_STR:
            entry->value.str_val = ATL_arena_strdup(&g_atl_config_arena, src->value.str_val);
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

ATL_ConfigEntry *ATL_config_table_get(ATL_ConfigTable *t, const char *key)
{
    if (!t)
    {
        ATL_errlog("%s: Config table is NULL", __func__);
        return NULL;
    }

    for (size_t i = 0; i < t->count; i++)
    {
        if (ATL_strmatch(t->entries[i].key, key))
        {
            return &t->entries[i];
        }
    }

    ATL_errlog("Key not found: %s", key);
    return NULL;
}

//----------------------------------------------------------------------------------------------------

ATL_ConfigTable *ATL_config_parse_file(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        ATL_errlog("Failed to open %s file to read", filename);
        ATL_warnlog("Does the file exists?");
        return NULL;
    }

    ATL_ConfigTable *t = atl_config_table_init();
    if (!t)
    {
        ATL_errlog("Failed to initialize ATL_ConfigTable");
        fclose(f);
        return NULL;
    }

    char line[ATL_CONFIG_MAX_LINE_PARSE];

    while (fgets(line, sizeof(line), f))
    {
        char *ptr = line;

        while (isspace((atl_u8) *ptr))
        {
            ptr++;
        }

        if (*ptr == '\0' || *ptr == '#' || *ptr == '\n')
        {
            continue;
        }

        atl_config_parse_line(ptr, t);
    }
    ATL_arena_stats(&g_atl_config_arena);
    fclose(f);
    return t;
}

//----------------------------------------------------------------------------------------------------

void atl_trim(char *s)
{
    char *start = s;
    char *end;

    while (isspace((atl_u8) *start))
    {
        start++;
    }

    if (*start == 0)
    {
        *s = 0;
        return;
    }

    end = start + strlen(start) - 1;
    while (end > start && isspace((atl_u8) *end))
    {
        end--;
    }
    *(end + 1) = '\0';

    if (start != s)
    {
        memmove(s, start, strlen(start) + 1);
    }
}

void atl_destroy_table(ATL_ConfigTable *t)
{
    if (t)
    {
        ATL_destroy_arena(&g_atl_config_arena);
    }
}
