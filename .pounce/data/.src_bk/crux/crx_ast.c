#include "crx_ast.h"
#include "crx.h"
#include <stdio.h>
#include <string.h>

CRX_AST_Node *crx_test_ast(void)
{
    CRX_AST_Node *p = BARR_arena_alloc(&g_crx_arena, sizeof(CRX_AST_Node));
    p->type = CRX_NODE_PROJECT;
    p->name = BARR_arena_strdup(&g_crx_arena, "aurora");

    p->child_count = 2;
    p->children = BARR_arena_alloc(&g_crx_arena, sizeof(CRX_AST_Node) * 2);

    CRX_AST_Node *assign = BARR_arena_alloc(&g_crx_arena, sizeof(CRX_AST_Node));
    assign->type = CRX_NODE_ASSIGNMENT;
    assign->name = BARR_arena_strdup(&g_crx_arena, "cflags");
    assign->arg_count = 1;
    assign->args = BARR_arena_alloc(&g_crx_arena, sizeof(char *) * assign->arg_count);
    assign->args[0] = BARR_arena_strdup(&g_crx_arena, "-Wall");

    CRX_AST_Node *print = BARR_arena_alloc(&g_crx_arena, sizeof(CRX_AST_Node));
    print->type = CRX_NODE_FN_CALL;
    print->name = BARR_arena_strdup(&g_crx_arena, "print");
    print->arg_count = 1;
    print->args = BARR_arena_alloc(&g_crx_arena, sizeof(char *) * print->arg_count);
    print->args[0] = BARR_arena_strdup(&g_crx_arena, "Hello World");

    p->children[0] = assign;
    p->children[1] = print;

    return p;
}

void CRX_eval_node(CRX_AST_Node *node)
{
    if (!node)
        return;

    switch (node->type)
    {
        case CRX_NODE_ASSIGNMENT:
            // For now, just print what would be assigned
            printf("[ASSIGN] %s = %s\n", node->name, node->args[0]);
            break;
        case CRX_NODE_FN_CALL:
            if (strcmp(node->name, "print") == 0)
            {
                printf("[PRINT] %s\n", node->args[0]);
            }
            break;
        case CRX_NODE_PROJECT:
            printf("[PROJECT] %s\n", node->name);
            for (size_t i = 0; i < node->child_count; i++)
            {
                CRX_eval_node(node->children[i]);
            }
            break;
        default:
            break;
    }
}
