#include "atl_arena.h"
#include "atl_io.h"
#include <stdio.h>

typedef struct
{
    int some_n;
    char *some_str;
} test;

int main(int argc, char **argv)
{
    printf("Hello, World from Atlas Build Manager 0.1.1\n");

    ATL_Arena arena;
    ATL_arena_init(&arena, 1024 * 1024 * 1024);

    test *t = ATL_arena_alloc(&arena, sizeof(test));
    t->some_n = 10;
    t->some_str = "test";
    test *a = ATL_arena_alloc(&arena, sizeof(test));
    test *b = ATL_arena_alloc(&arena, sizeof(test));
    test *c = ATL_arena_alloc(&arena, sizeof(test));
    ATL_arena_alloc(&arena, 1000);
    ATL_arena_alloc(&arena, 1024 * 1024);
    ATL_arena_alloc(&arena, 1024 * 1024 * 1024 * 1024LL);

    ATL_log("test_n = %d", t->some_n);
    ATL_log("some_str = %s", t->some_str);

    ATL_arena_stats(&arena);
    ATL_destroy_arena(&arena);

    ATL_VOID(a);
    ATL_VOID(b);
    ATL_VOID(c);

    (void) argc;
    (void) argv;
    return 0;
}
