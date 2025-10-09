#include "atl_project.h"
#include "atl_debug.h"
#include "atl_io.h"

#include <stdlib.h>
#include <string.h>

bool ATL_project_list_init(ATL_ProjectList *l, size_t initial_capacity)
{
    if (!l)
    {
        return false;
    }
    l->projects = malloc(sizeof(ATL_Project) * initial_capacity);
    if (!l->projects)
    {
        return false;
    }
    l->count = 0;
    l->capacity = initial_capacity;
    ATL_dbglog("%s(): initialized list", __func__);
    return true;
}

void ATL_destroy_project_list(ATL_ProjectList *l)
{
    if (!l || !l->projects)
    {
        return;
    }

    for (size_t i = 0; i < l->count; i++)
    {
        free((char *) l->projects[i].name);
        free((char *) l->projects[i].root_dir);
        free((char *) l->projects[i].build_type);
        free((char *) l->projects[i].compile_flags);
    }

    free(l->projects);
    l->projects = NULL;
    l->count = 0;
    l->capacity = 0;
}

ATL_Project *ATL_project_list_add(ATL_ProjectList *l, const char *name, const char *root_dir, const char *build_type,
                                  const char *compile_flags)
{
    if (!l)
    {
        return NULL;
    }

    if (l->count >= l->capacity)
    {
        size_t new_cap = l->capacity * 2;
        ATL_Project *tmp = realloc(l->projects, sizeof(ATL_Project) * new_cap);
        if (!tmp)
        {
            return NULL;
        }
        l->projects = tmp;
        l->capacity = new_cap;
    }

    ATL_Project *p = &l->projects[l->count];
    p->name = name ? strdup(name) : strdup(ATL_PROJECT_DEFAULT_NAME);
    if (!p->name)
    {
        goto fail;
    }
    p->root_dir = root_dir ? strdup(root_dir) : strdup(ATL_getcwd());
    if (!p->root_dir)
    {
        goto fail;
    }
    p->build_type = build_type ? strdup(build_type) : strdup(ATL_PROJECT_DEFAULT_BUILD_TYPE);
    if (!p->build_type)
    {
        goto fail;
    }
    p->compile_flags = compile_flags ? strdup(compile_flags) : strdup(ATL_PROJECT_DEFAULT_CFLAGS);
    if (!p->compile_flags)
    {
        goto fail;
    }

    p->id = l->count;
    l->count++;
    return p;

fail:
    free((char *) p->name);
    free((char *) p->root_dir);
    free((char *) p->build_type);
    free((char *) p->compile_flags);
    return NULL;
}

//----------------------------------------------------------------------------------------------------

ATL_Project *ATL_project_list_get_by_name(ATL_ProjectList *l, const char *name)
{
    if (!l)
    {
        return NULL;
    }
    for (size_t i = 0; i < l->count; i++)
    {
        if (ATL_strmatch(l->projects[i].name, name))
        {
            return &l->projects[i];
        }
    }
    return NULL;
}

ATL_Project *ATL_project_list_get_by_id(ATL_ProjectList *l, size_t id)
{
    if (!l || id == 0 || id >= l->count)
    {
        return NULL;
    }
    return &l->projects[id];
}
