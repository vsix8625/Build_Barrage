#ifndef ATL_PROJECT_H_
#define ATL_PROJECT_H_

#include "atl_defs.h"

typedef struct ATL_Project
{
    const char *name;
    const char *root_dir;

    const char *build_type;
    const char *compile_flags;
    size_t id;

    atl_i32 lua_source_table_ref;
} ATL_Project;

typedef struct ATL_ProjectList
{
    ATL_Project *projects;
    size_t count;
    size_t capacity;
} ATL_ProjectList;

#define ATL_PROJECT_DEFAULT_NAME "atlas_project"
#define ATL_PROJECT_DEFAULT_BUILD_TYPE "debug"
#define ATL_PROJECT_DEFAULT_CFLAGS "-Wall -Wextra -Werror -g -DDEBUG"

bool ATL_project_list_init(ATL_ProjectList *l, size_t initial_capacity);
ATL_Project *ATL_project_list_add(ATL_ProjectList *l, const char *name, const char *root_dir, const char *build_type,
                                  const char *compile_flags);
void ATL_destroy_project_list(ATL_ProjectList *l);
ATL_Project *ATL_project_list_get_by_name(ATL_ProjectList *l, const char *name);
ATL_Project *ATL_project_list_get_by_id(ATL_ProjectList *l, size_t id);

#endif  // ATL_PROJECT_H_
