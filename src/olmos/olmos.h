#ifndef OLMOS_H_
#define OLMOS_H_

#include "barr_arena.h"

#include <stddef.h>

#define OLM_EXPAND_BUF_SIZE (1024)
#define OLM_INITIAL_VARS    (1024)

typedef enum
{
    OLM_NODE_PROJECT,
    OLM_NODE_ASSIGNMENT,
    OLM_NODE_FN_CALL,
    OLM_NODE_UNKNOWN
} OLM_NodeType;

typedef struct OLM_Node
{
    struct OLM_Node **children;
    char            **args;
    char             *name;

    size_t arg_count;
    size_t child_count;

    OLM_NodeType type;
} OLM_Node;

typedef struct OLM_Var
{
    const char *key;
    const char *value;

    bool is_const;
} OLM_Var;

size_t    BARR_count_nodes(OLM_Node *node);
barr_i32  OLM_eval_config_node(OLM_Node *node, BARR_Arena *arena);
void      OLM_parse_vars(OLM_Node *root);
OLM_Node *OLM_parse_file(const char *file_path);

void        OLM_store_var(const char *key, const char *value);
const char *OLM_get_var(const char *key);
void        OLM_print_all_vars(void);

bool OLM_init(void);
bool OLM_close(void);

#endif  // OLMOS_H_
