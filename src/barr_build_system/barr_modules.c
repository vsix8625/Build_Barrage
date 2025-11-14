#include "barr_modules.h"
#include "barr_debug.h"
#include "barr_gc.h"
#include "barr_io.h"

//----------------------------------------------------------------------------------------------------
static BARR_Module g_barr_modules[BARR_MAX_MODULES];
static size_t g_barr_module_count = 0;

BARR_Module *BARR_get_module_array(void)
{
    return g_barr_modules;
}

size_t BARR_get_module_count(void)
{
    return g_barr_module_count;
}

//----------------------------------------------------------------------------------------------------

static bool barr_add_module_to_registry(const char *name, const char *path)
{
    if (name == NULL || path == NULL)
    {
        BARR_errlog("Module name or path is NULL");
        return false;
    }

    if (g_barr_module_count >= BARR_MAX_MODULES)
    {
        BARR_errlog("Maximum number of modules reached: %d", BARR_MAX_MODULES);
        return false;
    }

    g_barr_modules[g_barr_module_count].name = BARR_gc_strdup(name);
    g_barr_modules[g_barr_module_count].path = BARR_gc_strdup(path);
    g_barr_module_count++;

    BARR_dbglog("MODULE: name=%s:path=%s added to registry", name, path);
    return true;
}

BARR_Module *BARR_get_module(const char *name)
{
    for (size_t i = 0; i < g_barr_module_count; ++i)
    {
        if (BARR_strmatch(g_barr_modules[i].name, name))
        {
            return &g_barr_modules[i];
        }
    }

    return NULL;
}

void BARR_print_modules(void)
{
    BARR_log("Total modules: %zu", g_barr_module_count);
    for (size_t i = 0; i < g_barr_module_count; ++i)
    {
        BARR_log("\t%zu) %s at '%s/%s'", i + 1, g_barr_modules[i].name, BARR_getcwd(), g_barr_modules[i].path);
    }
}

bool BARR_add_module(const char *name, const char *path, const char *required)
{
    if (path == NULL)
    {
        BARR_errlog("Module path is NULL");
        return false;
    }

    if (name == NULL)
    {
        name = "barr_module";
    }

    bool is_required = false;
    if (required != NULL)
    {
        if (BARR_strmatch(required, "true") || BARR_strmatch(required, "yes") || BARR_strmatch(required, "required"))
        {
            is_required = true;
        }
    }

    char *self = BARR_get_self_exe();

    const char *args[] = {self, "build", "--dir", path, NULL};
    barr_i32 result = BARR_run_process(args[0], (char **) args, false);

    if (result != 0)
    {
        BARR_errlog("%s(): failed to build %s", __func__, path);
        if (is_required)
        {
            BARR_errlog("Module build is required");
            return false;
        }
    }

    if (!barr_add_module_to_registry(name, path))
    {
        BARR_errlog("Failed to register module %s at %s", name, path);
        return false;
    }

    return true;
}
