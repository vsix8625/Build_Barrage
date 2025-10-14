#ifndef CRX_AST_H_
#define CRX_AST_H_

#include <stddef.h>

typedef enum
{
    CRX_NODE_PROJECT,
    CRX_NODE_ASSIGNMENT,
    CRX_NODE_FN_CALL,
    CRX_NODE_UNKNOWN
} CRX_NodeType;

typedef struct CRX_AST_Node
{
    CRX_NodeType type;
    char *name;
    char **args;
    size_t arg_count;
    struct CRX_AST_Node **children;
    size_t child_count;
} CRX_AST_Node;

CRX_AST_Node *crx_test_ast(void);
void CRX_eval_node(CRX_AST_Node *node);

#endif  // CRX_AST_H_
