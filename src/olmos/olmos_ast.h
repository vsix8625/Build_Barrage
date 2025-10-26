#ifndef OLMOS_AST_H_
#define OLMOS_AST_H_

#include <stddef.h>

typedef enum
{
    OLM_NODE_PROJECT,
    OLM_NODE_ASSIGNMENT,
    OLM_NODE_FN_CALL,
    OLM_NODE_UNKNOWN
} OLM_NodeType;

typedef struct OLM_AST_Node
{
    OLM_NodeType type;
    char *name;
    char **args;
    size_t arg_count;
    struct OLM_AST_Node **children;
    size_t child_count;
} OLM_AST_Node;

typedef struct OLM_Var
{
    const char *key;
    const char *value;
} OLM_Var;

void OLM_eval_node(OLM_AST_Node *node);
OLM_AST_Node *OLM_parse_file(const char *file_path);
const char *OLM_get_var(const char *key);

#endif  // OLMOS_AST_H_
