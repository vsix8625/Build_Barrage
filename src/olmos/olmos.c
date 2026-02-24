#include "olmos.h"
#include "barr_debug.h"
#include "barr_env.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "barr_modules.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xxhash.h>

static OLM_Var *g_olm_vars         = NULL;
static size_t   g_olm_var_capacity = 0;
static size_t   g_olm_var_count    = 0;

static void olm_reset_vars(void)
{
    g_olm_var_count = 0;
}

//--------------------------------------------------------------------------------------------------

static void  olm_trim(char *s);
static char *olm_strip_quotes(const char *s);

//--------------------------------------------------------------------------------------------------

void OLM_print_all_vars(void)
{
    for (size_t i = 0; i < g_olm_var_count; ++i)
    {
        printf("\t%-24s = %s\n", g_olm_vars[i].key, g_olm_vars[i].value);
    }
}

void OLM_store_var_const(const char *key, const char *value)
{
    if (g_olm_var_count >= g_olm_var_capacity)
    {
        size_t   new_cap = g_olm_var_capacity == 0 ? OLM_INITIAL_VARS : g_olm_var_capacity * 2;
        OLM_Var *tmp     = BARR_gc_realloc(g_olm_vars, sizeof(OLM_Var) * new_cap);
        if (tmp == NULL)
        {
            return;
        }
        g_olm_vars         = tmp;
        g_olm_var_capacity = new_cap;
    }
    g_olm_vars[g_olm_var_count++] =
        (OLM_Var) {.key = BARR_gc_strdup(key), .value = BARR_gc_strdup(value), .is_const = true};
}

void OLM_store_var(const char *key, const char *value)
{
    for (size_t i = 0; i < g_olm_var_count; i++)
    {
        if (BARR_strmatch(g_olm_vars[i].key, key) && g_olm_vars[i].is_const)
        {
            BARR_warnlog("Cannot assign to constant: %s", key);
            return;
        }
    }

    if (g_olm_var_count >= g_olm_var_capacity)
    {
        size_t   new_cap = g_olm_var_capacity == 0 ? OLM_INITIAL_VARS : g_olm_var_capacity * 2;
        OLM_Var *tmp     = BARR_gc_realloc(g_olm_vars, sizeof(OLM_Var) * new_cap);
        if (tmp == NULL)
        {
            return;
        }
        g_olm_vars         = tmp;
        g_olm_var_capacity = new_cap;
    }
    g_olm_vars[g_olm_var_count++] =
        (OLM_Var) {.key = BARR_gc_strdup(key), .value = BARR_gc_strdup(value), .is_const = false};
}

const char *OLM_get_var(const char *key)
{
    for (size_t i = 0; i < g_olm_var_count; i++)
    {
        if (BARR_strmatch(g_olm_vars[i].key, key))
        {
            return g_olm_vars[i].value;
        }
    }
    return NULL;
}

static char *olm_expand_vars(const char *input)
{
    if (input == NULL || !*input)
    {
        return BARR_gc_strdup("");
    }

    char *buffer = malloc(OLM_EXPAND_BUF_SIZE);
    if (buffer == NULL)
    {
        return BARR_gc_strdup("");
    }
    char       *out = buffer;
    const char *p   = input;

    while (*p && (size_t) (out - buffer) < OLM_EXPAND_BUF_SIZE - 1)
    {
        if (*p == '$')
        {
            p++;

            char  var_name[BARR_BUF_SIZE_1024] = {0};
            char *vn                           = var_name;

            if (*p == '{')
            {
                p++;
                while (*p && *p != '}' && (isalnum((barr_u8) *p) || *p == '_'))
                {
                    *vn++ = *p++;
                }

                if (*p == '}')
                {
                    p++;
                }
                else
                {
                    *out++ = '$';
                    *out++ = '{';
                    memcpy(out, var_name, strlen(var_name));
                    out += strlen(var_name);
                    continue;
                }
            }
            else
            {
                while (*p && (isalnum((barr_u8) *p) || *p == '_'))
                {
                    *vn++ = *p++;
                }
            }

            *vn = '\0';

            if (var_name[0])
            {
                const char *val = OLM_get_var(var_name);
                if (val)
                {
                    size_t len       = strlen(val);
                    size_t remaining = OLM_EXPAND_BUF_SIZE - (out - buffer);
                    if (len < remaining)
                    {
                        memcpy(out, val, len);
                        out += len;
                    }
                }
                else
                {
                    size_t var_len   = strlen(var_name);
                    size_t remaining = OLM_EXPAND_BUF_SIZE - (out - buffer);
                    if (remaining >= 1)
                    {
                        *out++ = '$';
                        remaining--;
                    }
                    if (var_len <= remaining)
                    {
                        memcpy(out, var_name, var_len);
                        out += var_len;
                    }
                }
            }
            else
            {
                *out++ = '$';
            }
        }
        else
        {
            *out++ = *p++;
        }
    }

    *out         = '\0';
    char *result = BARR_gc_strdup(buffer);
    free(buffer);
    return result;
}

//--------------------------------------------------------------------------------------------------

void OLM_parse_vars(OLM_Node *root)
{
    if (root == NULL)
    {
        return;
    }

    for (size_t i = 0; i < root->child_count; i++)
    {
        OLM_Node *node = root->children[i];
        if (!node)
        {
            continue;
        }

        if (node->type == OLM_NODE_ASSIGNMENT && node->arg_count > 0 && node->args[0])
        {
            char *expanded = olm_expand_vars(node->args[0]);
            OLM_store_var(node->name, expanded);
        }
    }
}

static bool olm_parse_string_args(const char *src, char ***out_args, size_t *out_count)
{
    if (src == NULL || out_args == NULL || out_count == NULL)
    {
        return false;
    }

    const char *p     = src;
    size_t      cap   = 4;
    size_t      count = 0;
    char      **args  = BARR_gc_alloc(sizeof(char *) * cap);
    if (args == NULL)
    {
        return false;
    }

    for (;;)
    {
        // skip leading whitespace
        while (*p && isspace((unsigned char) *p))
        {
            p++;
        }

        // end of string -> done
        if (*p == '\0')
        {
            break;
        }

        // must start with a quote
        if (*p != '"' && *p != '\'')
        {
            return false;
        }

        char        quote = *p++;
        const char *start = p;

        // find matching closing quote
        while (*p && *p != quote)
        {
            // allow escaped quotes: \" or \'
            if (*p == '\\' && p[1] == quote)
            {
                p += 2;
                continue;
            }
            p++;
        }

        // no matching quote -> syntax error
        if (*p != quote)
        {
            return false;
        }

        size_t len = (size_t) (p - start);
        char  *s   = BARR_gc_alloc(len + 1);
        if (s == NULL)
        {
            return false;
        }

        // copy and unescape simple \" or \'
        size_t di = 0;
        for (const char *q = start; q < p; ++q)
        {
            if (*q == '\\' && (q + 1) < p && (*(q + 1) == '"' || *(q + 1) == '\''))
            {
                s[di++] = *(q + 1);
                ++q;
            }
            else
            {
                s[di++] = *q;
            }
        }
        s[di] = '\0';

        // push arg (grow if needed)
        if (count + 1 >= cap)
        {
            cap        *= 2;
            char **tmp  = BARR_gc_realloc(args, sizeof(char *) * cap);
            if (tmp == NULL)
            {
                return false;
            }
            args = tmp;
        }

        args[count++] = s;

        p++;  // skip closing quote

        // skip whitespace after arg
        while (*p && isspace((unsigned char) *p))
        {
            p++;
        }

        // if comma, skip and continue to next arg
        if (*p == ',')
        {
            p++;
            continue;
        }

        // otherwise we should be at end
        while (*p && isspace((unsigned char) *p))
        {
            p++;
        }
        if (*p == '\0')
        {
            break;
        }

        // stray characters -> syntax error
        return false;
    }

    // null-terminate args list
    if (count + 1 >= cap)
    {
        char **tmp = BARR_gc_realloc(args, sizeof(char *) * (count + 1));
        if (tmp == NULL)
        {
            return false;
        }
        args = tmp;
    }
    args[count] = NULL;

    *out_args  = args;
    *out_count = count;
    return true;
}

static char *barr_find_unquoted_eq(const char *s)
{
    barr_i32 in_single = 0;
    barr_i32 in_double = 0;

    for (size_t i = 0; s[i]; ++i)
    {
        if (s[i] == '\'' && !in_double)
        {
            in_single = !in_single;
        }
        else if (s[i] == '"' && !in_single)
        {
            in_double = !in_double;
        }
        else if (s[i] == '=' && !in_single && !in_double)
        {
            return (char *) (s + i);
        }
    }

    return NULL;
}

OLM_Node *OLM_parse_file(const char *file_path)
{
    FILE *fp = fopen(file_path, "r");
    if (!fp)
    {
        BARR_errlog("(%s): failed to open %s", __func__, file_path);
        return NULL;
    }

    OLM_Node *root = BARR_gc_alloc(sizeof(OLM_Node));

    if (!root)
    {
        BARR_errlog("Config parser returned NULL");
        return NULL;
    }

    root->type        = OLM_NODE_PROJECT;
    root->name        = BARR_gc_strdup("root");
    root->child_count = 0;
    root->children    = NULL;

    size_t line_n = 0;
    char   line[BARR_BUF_SIZE_1024];
    while (fgets(line, sizeof(line), fp))
    {
        line_n++;

        olm_trim(line);

        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        /* Find the statement terminator ';' ---- */
        char *semicolon = strchr(line, ';');
        if (!semicolon)
        {
            BARR_errlog("%s(): missing ';' at line %zu", __func__, line_n);
            fclose(fp);
            return NULL;
        }

        *semicolon = '\0';

        olm_trim(line);

        if (line[0] == '\0')
        {
            continue;
        }

        OLM_Node *node = BARR_gc_alloc(sizeof(OLM_Node));
        if (!node)
        {
            fclose(fp);
            return NULL;
        }
        memset(node, 0, sizeof(*node));

        char *eq = barr_find_unquoted_eq(line);
        if (eq)
        {
            *eq = '\0';
            olm_trim(line);

            char *value_start = eq + 1;
            olm_trim(value_start);

            size_t val_len = strlen(value_start);

            if (val_len >= 2 && !((value_start[0] == '"' && value_start[val_len - 1] == '"') ||
                                  (value_start[0] == '\'' && value_start[val_len - 1] == '\'')))
            {
                value_start[val_len - 1] = '\0';
                value_start++;
            }

            node->type      = OLM_NODE_ASSIGNMENT;
            node->name      = BARR_gc_strdup(line);
            node->arg_count = 1;
            node->args      = BARR_gc_alloc(sizeof(char *));
            if (node->args == NULL)
            {
                fclose(fp);
                return NULL;
            }
            node->args[0] = olm_strip_quotes(value_start);
        }
        else if (strncmp(line, "print", 5) == 0)
        {
            char *start = strchr(line, '(');
            char *end   = strrchr(line, ')');
            if (!start || !end || end <= start)
            {
                BARR_errlog(
                    "%s(): syntax error at line %s:%zu, invalid print()", __func__, line, line_n);
                fclose(fp);
                return NULL;
            }

            *end = '\0';
            start++;
            olm_trim(start);

            size_t val_len = strlen(start);
            if (val_len >= 2 && ((start[0] == '"' && start[val_len - 1] == '"') ||
                                 (start[0] == '\'' && start[val_len - 1] == '\'')))
            {
                start[val_len - 1] = '\0';
                start++;
            }

            node->type      = OLM_NODE_FN_CALL;
            node->name      = BARR_gc_strdup("print");
            node->arg_count = 1;
            node->args      = BARR_gc_alloc(sizeof(char *));
            if (node->args == NULL)
            {
                fclose(fp);
                return NULL;
            }
            node->args[0] = BARR_gc_strdup(start);
        }
        else if (strncmp(line, "find_package", 12) == 0)
        {
            char *start = strchr(line, '(');
            char *end   = strrchr(line, ')');

            if (start == NULL || end == NULL || end <= start)
            {
                BARR_errlog("%s(): syntax error at line %s:%zu, invalid find_package()",
                            __func__,
                            line,
                            line_n);
                fclose(fp);
                return NULL;
            }

            *end = '\0';
            start++;

            olm_trim(start);
            size_t len = strlen(start);
            if (len >= 2 && start[0] == '"' && start[len - 1] == '"')
            {
                start[len - 1] = '\0';
                start++;
            }

            node->type      = OLM_NODE_FN_CALL;
            node->name      = BARR_gc_strdup("find_package");
            node->arg_count = 1;
            node->args      = BARR_gc_alloc(sizeof(char *));
            if (node->args == NULL)
            {
                fclose(fp);
                return NULL;
            }
            node->args[0] = BARR_gc_strdup(start);

            BARR_dbglog("Package: %s parsed", node->args[0]);
        }
        else if (strncmp(line, "run_cmd", 7) == 0)
        {
            char *start = strchr(line, '(');
            char *end   = strrchr(line, ')');

            if (start == NULL || end == NULL || end <= start)
            {
                BARR_errlog(
                    "%s(): syntax error at line %s:%zu, invalid run_cmd()", __func__, line, line_n);
                fclose(fp);
                return NULL;
            }

            *end = '\0';
            start++;

            olm_trim(start);

            size_t len = strlen(start);
            if (len >= 2 && ((start[0] == '"' && start[len - 1] == '"') ||
                             (start[0] == '\'' && start[len - 1] == '\'')))
            {
                start[len - 1] = '\0';
                start++;
            }

            node->type      = OLM_NODE_FN_CALL;
            node->name      = BARR_gc_strdup("run_cmd");
            node->arg_count = 1;
            node->args      = BARR_gc_alloc(sizeof(char *));
            if (node->args == NULL)
            {
                fclose(fp);
                return NULL;
            }
            node->args[0] = BARR_gc_strdup(start);
        }
        else if (strncmp(line, "file_write", 10) == 0)
        {
            char *start = strchr(line, '(');
            char *end   = strrchr(line, ')');

            if (start == NULL || end == NULL || end <= start)
            {
                BARR_errlog("%s(): syntax error at line %s:%zu, invalid file_write()",
                            __func__,
                            line,
                            line_n);
                fclose(fp);
                return NULL;
            }

            *end = '\0';
            start++;

            olm_trim(start);

            // parse quoted args inside parentheses
            char **args = NULL;
            size_t argc = 0;
            if (!olm_parse_string_args(start, &args, &argc) || argc < 2 || argc > 3)
            {
                BARR_errlog("%s(): file_write() requires: content, filename[, append|true]",
                            __func__);
                fclose(fp);
                return NULL;
            }

            node->type      = OLM_NODE_FN_CALL;
            node->name      = BARR_gc_strdup("file_write");
            node->arg_count = argc;
            node->args      = BARR_gc_alloc(sizeof(char *) * argc);
            for (size_t i = 0; i < argc; ++i)
            {
                node->args[i] = BARR_gc_strdup(args[i]);
            }

            BARR_dbglog("file_write parsed: arg_count=%zu", argc);
        }
        else if (strncmp(line, "add_module", 10) == 0)
        {
            char *start = strchr(line, '(');
            char *end   = strrchr(line, ')');

            if (start == NULL || end == NULL || end <= start)
            {
                BARR_errlog("%s(): syntax error at line %s:%zu, invalid add_module()",
                            __func__,
                            line,
                            line_n);
                fclose(fp);
                return NULL;
            }

            *end = '\0';
            start++;

            olm_trim(start);

            // parse quoted args inside parentheses
            char **args = NULL;
            size_t argc = 0;
            if (!olm_parse_string_args(start, &args, &argc))
            {
                BARR_errlog("%s(): invalid arguments to add_module() at line %s:%zu",
                            __func__,
                            line,
                            line_n);
                fclose(fp);
                return NULL;
            }

            node->type      = OLM_NODE_FN_CALL;
            node->name      = BARR_gc_strdup("add_module");
            node->arg_count = argc;
            node->args      = BARR_gc_alloc(sizeof(char *) * argc);
            if (node->args == NULL)
            {
                fclose(fp);
                return NULL;
            }
            for (size_t i = 0; i < argc; ++i)
            {
                node->args[i] = BARR_gc_strdup(args[i]);
            }

            BARR_dbglog("add_module parsed: arg_count=%zu", argc);
        }
        else if (strncmp(line, "project", 7) == 0)
        {
            node->type = OLM_NODE_PROJECT;
            char *arg  = line + 7;
            olm_trim(arg);

            size_t val_len = strlen(arg);
            if (val_len < 2 || !((arg[0] == '"' && arg[val_len - 1] == '"') ||
                                 (arg[0] == '\'' && arg[val_len - 1] == '\'')))
            {
                BARR_errlog("%s(): syntax error at line %s:%zu, unmatched quotes in project()",
                            __func__,
                            line,
                            line_n);
                fclose(fp);
                return NULL;
            }

            arg[val_len - 1] = '\0';
            arg++;

            node->name = BARR_gc_strdup(arg);
        }
        else
        {
            node->type = OLM_NODE_UNKNOWN;
            node->name = BARR_gc_strdup(line);
        }

        root->child_count++;
        root->children = BARR_gc_realloc(root->children, sizeof(OLM_Node *) * root->child_count);
        if (!root->children)
        {
            fclose(fp);
            return NULL;
        }
        root->children[root->child_count - 1] = node;
    }

    fclose(fp);
    return root;
}

//--------------------------------------------------------------------------------------------------

size_t BARR_count_nodes(OLM_Node *node)
{
    if (node == NULL)
    {
        return 0;
    }

    size_t count = 1;
    if (node->type == OLM_NODE_PROJECT)
    {
        for (size_t i = 0; i < node->child_count; i++)
        {
            count += BARR_count_nodes(node->children[i]);
        }
    }
    return count;
}

barr_i32 OLM_eval_config_node(OLM_Node *root, BARR_Arena *arena)
{
    time_t start = time(NULL);

    const time_t max_seconds = 60;

    if (root == NULL || arena == NULL)
    {
        return 1;
    }

    size_t stack_capacity = BARR_count_nodes(root);

    OLM_Node **stack = (OLM_Node **) BARR_arena_alloc(arena, sizeof(OLM_Node *) * stack_capacity);

    if (stack == NULL)
    {
        return 1;
    }

    size_t top = 0;
    if (stack_capacity > 0)
    {
        stack[top++] = root;
    }

    while (top > 0)
    {
        if (difftime(time(NULL), start) > max_seconds)
        {
            BARR_errlog("%s(): execution time exceeded %lds", __func__, max_seconds);
            return 1;
        }

        if (top >= stack_capacity)
        {
            BARR_errlog("Eval stack overflow: %zu >= %zu", top, stack_capacity);
            return 1;
        }

        OLM_Node *node = stack[--top];
        if (node == NULL)
        {
            continue;
        }

        switch (node->type)
        {
            case OLM_NODE_ASSIGNMENT:
                char *expandend = olm_expand_vars(node->args[0]);
                OLM_store_var(node->name, expandend);
                break;
            case OLM_NODE_FN_CALL:

                if (node->arg_count > 0 && node->args)
                {
                    for (size_t i = 0; i < node->arg_count; ++i)
                    {
                        node->args[i] = olm_expand_vars(node->args[i]);
                    }
                }

                if (BARR_strmatch(node->name, "print"))
                {
                    BARR_printf("%s\n", node->args[0]);
                }

                if (BARR_strmatch(node->name, "run_cmd"))
                {
                    const char *cmd = node->args[0];

                    if (g_barr_trust == false)
                    {
                        BARR_printf("Barr wants to run: \033[38;5;202m%s\033[0m. Allow? (y/N): ",
                                    cmd);
                        fflush(stdout);

                        char ans[4];
                        if (fgets(ans, sizeof(ans), stdin) == NULL ||
                            (ans[0] != 'y' && ans[0] != 'Y'))
                        {
                            BARR_warnlog("run_cmd skipped by user: %s", cmd);
                            return 1;
                        }
                    }

                    char cmd_copy[1024];
                    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
                    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
                    char *first_word               = strtok(cmd_copy, " \t");

                    const char *self = basename(BARR_get_self_exe());

                    if (strstr(first_word, self))
                    {
                        BARR_errlog("%s(): barr cannot invoke itself via run_cmd()", __func__);
                        return 1;
                    }

                    char *argv[] = {"/usr/bin/bash", "-c", (char *) cmd, NULL};

                    BARR_run_process(argv[0], argv, false);
                }

                if (BARR_strmatch(node->name, "file_write"))
                {
                    if (node->arg_count < 2 || node->arg_count > 3)
                    {
                        BARR_errlog("file_write() require: content, filename[, append|true]");
                        return 1;
                    }

                    const char *content  = node->args[0];
                    const char *filename = node->args[1];

                    bool do_append = false;

                    if (node->arg_count == 3)
                    {
                        if (BARR_strmatch(node->args[2], "append") ||
                            BARR_strmatch(node->args[2], "true"))
                        {
                            do_append = true;
                        }
                        else
                        {
                            BARR_errlog("file_write(): third argument must be 'append' or 'true'");
                            return 1;
                        }
                    }

                    barr_i32 result = 0;

                    if (do_append)
                    {
                        result = BARR_file_append(filename, "%s\n", content);
                    }
                    else
                    {
                        result = BARR_file_write(filename, "%s\n", content);
                    }

                    if (result < 0)
                    {
                        BARR_errlog("file_write() failed for %s", filename);
                        return 1;
                    }
                }

                if (BARR_strmatch(node->name, "add_module"))
                {
                    if (node->arg_count < 2)
                    {
                        BARR_errlog("add_module() requires at least 2 args: name, path");
                        return 1;
                    }

                    const char *name     = node->args[0];
                    const char *path     = node->args[1];
                    const char *required = NULL;

                    if (node->arg_count >= 3)
                    {
                        required = node->args[2];
                    }

                    BARR_dbglog("Module args: name=%s, path=%s, required=%s", name, path, required);

                    if (!BARR_add_module(name, path, required))
                    {
                        return 101;
                    }
                }

                break;
            case OLM_NODE_PROJECT:
                for (ssize_t i = node->child_count - 1; i >= 0; --i)
                {
                    stack[top++] = node->children[i];
                }
                break;
            default:
                break;
        }
    }
    return 0;
}

//--------------------------------------------------------------------------------------------------

static void olm_trim(char *s)
{
    char *start = s;
    char *end;

    while (isspace((barr_u8) *start))
    {
        start++;
    }

    if (*start == 0)
    {
        *s = 0;
        return;
    }

    end = start + strlen(start) - 1;
    while (end > start && isspace((barr_u8) *end))
    {
        end--;
    }
    *(end + 1) = '\0';

    if (start != s)
    {
        memmove(s, start, strlen(start) + 1);
    }
}

static char *olm_strip_quotes(const char *s)
{
    if (!s || !*s)
    {
        return BARR_gc_strdup("");
    }

    size_t len = strlen(s);
    if ((s[0] == '"' && s[len - 1] == '"') || (s[0] == '\'' && s[len - 1] == '\''))
    {
        char *copy = BARR_gc_alloc(len - 1);
        memcpy(copy, s + 1, len - 2);
        copy[len - 2] = '\0';
        return copy;
    }

    return BARR_gc_strdup(s);  // no quotes, copy as-is
}

//---------------------------------------------------------------------------------------

static char *olm_barr_build_compiler(void)
{
#if defined(__clang__)
    return BARR_which("clang");
#elif defined(__GNUC__)
    return BARR_which("gcc");
#elif defined(__MSC_VER)
    return "msvc";
#else
    return "unknown";
#endif
}

static void olm_detect_compilers(void)
{
    static const char *compiler_list[] = {
        "gcc", "g++", "clang", "clang++", "cc", "c++", "clang-18", "clang-17", "tcc", "zig", NULL};

    for (size_t i = 0; compiler_list[i]; ++i)
    {
        char *value = BARR_find_in_path(compiler_list[i]);

        if (value)
        {
            char   key[BARR_BUF_SIZE_64];
            char  *str = strdup(compiler_list[i]);
            size_t len = strlen(str);

            for (size_t j = 0; j < len; ++j)
            {
                if (str[j] == '+' && str[j + 1] == '+')
                {
                    str[j]     = 'p';
                    str[j + 1] = 'p';
                    j++;
                }
            }

            for (size_t j = 0; str[j]; ++j)
            {
                str[j] = (char) toupper((barr_u8) str[j]);
            }

            snprintf(key, sizeof(key), "BARR_COMPILER_%s", str);
            OLM_store_var_const(key, value);

            free(str);
        }
    }
}

static void olm_load_constants(void)
{
    olm_barr_build_compiler();

    OLM_store_var_const("BARR_OS_NAME", BARR_OS_NAME);
    OLM_store_var_const("BARR_OS_VESRION", BARR_get_config("version"));
    OLM_store_var_const("BARR_OS_MACHINE", BARR_get_config("machine"));
    OLM_store_var_const("BARR_CWD", BARR_getcwd());

    //---------------------------------------

    char  data_buf[BARR_PATH_MAX];
    char *data_dir = getenv("XDG_DATA_DIR");

    if (data_dir == NULL)
    {
        snprintf(data_buf, sizeof(data_buf), "%s/%s", BARR_GET_HOME(), ".local/share/barr");
    }
    else
    {
        snprintf(data_buf, sizeof(data_buf), "%s/barr", data_dir);
    }

    OLM_store_var_const("BARR_DATA_DIR", data_buf);

    //---------------------------------------

    OLM_store_var_const("BARR_VERSION", BARR_version_get_str());
    OLM_store_var_const("BARR_COMPILER", olm_barr_build_compiler());

    olm_detect_compilers();
}

//---------------------------------------------------------------------------------------

bool OLM_init(void)
{
    olm_reset_vars();
    if (g_barr_verbose)
    {
        BARR_log("Config parser system initialized");
    }
    olm_load_constants();
    return true;
}

bool OLM_close(void)
{
    if (g_barr_verbose)
    {
        BARR_log("Config system shutdown, sweeping %zu variables into the void...",
                 g_olm_var_count);
    }
    olm_reset_vars();
    return true;
}
