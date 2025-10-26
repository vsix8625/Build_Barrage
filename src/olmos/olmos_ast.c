#include "olmos_ast.h"
#include "barr_debug.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "olmos.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static OLM_Var g_olm_vars[BARR_BUF_SIZE_1024];
static size_t g_olm_var_count = 0;

static void OLM_store_var(const char *key, const char *value)
{
    g_olm_vars[g_olm_var_count++] = (OLM_Var) {key, value};
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

OLM_AST_Node *OLM_parse_file(const char *file_path)
{
    FILE *fp = fopen(file_path, "r");
    if (!fp)
    {
        BARR_errlog("(%s): failed to open %s", __func__, file_path);
        return NULL;
    }

    OLM_AST_Node *root = BARR_gc_alloc(sizeof(OLM_AST_Node));
    if (!root)
    {
        return NULL;
    }
    root->type = OLM_NODE_PROJECT;
    root->name = BARR_gc_strdup("root");
    root->child_count = 0;
    root->children = NULL;

    size_t line_n = 0;
    char line[BARR_BUF_SIZE_1024];
    while (fgets(line, sizeof(line), fp))
    {
        line_n++;
        olm_trim(line);
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        size_t len = strlen(line);
        if (len < 3 || line[len - 1] != ';')
        {
            BARR_errlog("%s(): syntax error at line %s:%zu", __func__, line, line_n);
            fclose(fp);
            return NULL;
        }
        line[len - 1] = '\0';

        OLM_AST_Node *node = BARR_gc_alloc(sizeof(OLM_AST_Node));
        if (!node)
        {
            fclose(fp);
            return NULL;
        }
        memset(node, 0, sizeof(*node));

        char *eq = strchr(line, '=');
        if (eq)
        {
            *eq = '\0';
            olm_trim(line);
            olm_trim(eq + 1);

            char *value_start = eq + 1;
            size_t val_len = strlen(value_start);
            if (val_len < 2 || !((value_start[0] == '"' && value_start[val_len - 1] == '"') ||
                                 (value_start[0] == '\'' && value_start[val_len - 1] == '\'')))
            {
                BARR_errlog("%s(): syntax error at line %s:%zu", __func__, line, line_n);
                fclose(fp);
                return NULL;
            }

            value_start[val_len - 1] = '\0';
            value_start++;

            node->type = OLM_NODE_ASSIGNMENT;
            node->name = BARR_gc_strdup(line);
            node->arg_count = 1;
            node->args = BARR_gc_alloc(sizeof(char *));
            if (!node->args)
            {
                fclose(fp);
                return NULL;
            }
            node->args[0] = olm_strip_quotes(value_start);
        }
        else if (strncmp(line, "print", 5) == 0)
        {
            char *start = strchr(line, '(');
            char *end = strrchr(line, ')');
            if (!start || !end || end <= start)
            {
                BARR_errlog("%s(): syntax error at line %s:%zu, invalid print()", __func__, line, line_n);
                fclose(fp);
                return NULL;
            }

            *end = '\0';
            start++;
            olm_trim(start);

            size_t val_len = strlen(start);
            if (val_len < 2 ||
                !((start[0] == '"' && start[val_len - 1] == '"') || (start[0] == '\'' && start[val_len - 1] == '\'')))
            {
                BARR_errlog("%s(): syntax error at line %s:%zu, unmatched quote in print()", __func__, line, line_n);
                fclose(fp);
                return NULL;
            }

            start[val_len - 1] = '\0';
            start++;

            node->type = OLM_NODE_FN_CALL;
            node->name = BARR_gc_strdup("print");
            node->arg_count = 1;
            node->args = BARR_gc_alloc(sizeof(char *));
            node->args[0] = BARR_gc_strdup(start);
        }
        else if (strncmp(line, "find_package", 12) == 0)
        {
            char *start = strchr(line, '(');
            char *end = strchr(line, ')');

            if (!start || !end || end <= start)
            {
                BARR_errlog("%s(): syntax error at line %s:%zu, invalid find_package()", __func__, line, line_n);
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

            node->type = OLM_NODE_FN_CALL;
            node->name = BARR_gc_strdup("find_package");
            node->arg_count = 1;
            node->args = BARR_gc_alloc(sizeof(char *));
            node->args[0] = BARR_gc_strdup(start);

            BARR_dbglog("Package: %s parsed", node->args[0]);
        }
        // NOTE: project node currently only just parse quoted strings
        // NOTE: will be used in future update for multi targets/projects
        else if (strncmp(line, "project", 7) == 0)
        {
            node->type = OLM_NODE_PROJECT;
            char *arg = line + 7;
            olm_trim(arg);

            size_t val_len = strlen(arg);
            if (val_len < 2 ||
                !((arg[0] == '"' && arg[val_len - 1] == '"') || (arg[0] == '\'' && arg[val_len - 1] == '\'')))
            {
                BARR_errlog("%s(): syntax error at line %s:%zu, unmatched quotes in project()", __func__, line, line_n);
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
        root->children = BARR_gc_realloc(root->children, sizeof(OLM_AST_Node *) * root->child_count);
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

void OLM_eval_node(OLM_AST_Node *node)
{
    if (!node)
        return;

    switch (node->type)
    {
        case OLM_NODE_ASSIGNMENT:
            OLM_store_var(node->name, node->args[0]);
            OLM_get_var(node->name);
            break;
        case OLM_NODE_FN_CALL:
            if (BARR_strmatch(node->name, "print"))
            {
                printf("%s\n", node->args[0]);
            }
            break;
        case OLM_NODE_PROJECT:
            for (size_t i = 0; i < node->child_count; i++)
            {
                OLM_eval_node(node->children[i]);
            }
            break;
        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
