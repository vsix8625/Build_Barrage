#include "barr_modules.h"
#include "barr_debug.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "olmos.h"
#include "olmos_variables.h"
#include <string.h>

//----------------------------------------------------------------------------------------------------
static BARR_Module g_barr_modules[BARR_MAX_MODULES];
static size_t      g_barr_module_count = 0;

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
        BARR_log("\t%zu) %s at '%s/%s'",
                 i + 1,
                 g_barr_modules[i].name,
                 BARR_getcwd(),
                 g_barr_modules[i].path);
    }
}

static void barr_module_staging(const char *module)
{
    // parse Barrfile vars
    OLM_Node *root = OLM_parse_file("Barrfile");
    if (root == NULL)
    {
        BARR_errlog("%s(): failed to parse Barrfile", __func__);
        return;
    }

    OLM_parse_vars(root);

    const char *target = OLM_get_var(OLM_VAR_TARGET);
    if (target == NULL)
    {
        target = "barr_default";
        BARR_warnlog(
            "%s(): TARGET not set in 'Barrfile' default %s will be used", __func__, target);
    }

    const char *version = OLM_get_var(OLM_VAR_VERSION);
    if (version == NULL)
    {
        version = "0.0.1";
        BARR_warnlog(
            "%s(): VERSION not set in 'Barrfile' default %s will be used", __func__, version);
    }

    const char *out_dir_var = OLM_get_var(OLM_VAR_OUT_DIR);

    char out_dir[BARR_BUF_SIZE_4096];
    if (out_dir_var == NULL)
    {
        snprintf(out_dir, sizeof(out_dir), "%s/build/debug", BARR_getcwd());
        BARR_warnlog(
            "%s(): OUT_DIR not set in 'Barrfile' default %s will be used", __func__, out_dir);
    }
    else
    {
        snprintf(out_dir, sizeof(out_dir), "%s", out_dir_var);
    }

    char staging_dir[BARR_PATH_MAX];
    snprintf(staging_dir, sizeof(staging_dir), "%s/share/%s-%s", out_dir, target, version);

    BARR_mkdir_p(staging_dir);

    char module_dir[BARR_PATH_MAX * 2];
    snprintf(module_dir, sizeof(module_dir), "%s/%s", staging_dir, module);

    char stage_mod_bin_dir[BARR_PATH_MAX * 3];
    snprintf(stage_mod_bin_dir, sizeof(stage_mod_bin_dir), "%s/bin", module_dir);

    char stage_mod_lib_dir[BARR_PATH_MAX * 3];
    snprintf(stage_mod_lib_dir, sizeof(stage_mod_lib_dir), "%s/lib", module_dir);

    char stage_mod_inc_dir[BARR_PATH_MAX * 3];
    snprintf(stage_mod_inc_dir, sizeof(stage_mod_inc_dir), "%s/include", module_dir);

    BARR_mkdir_p(stage_mod_bin_dir);
    BARR_mkdir_p(stage_mod_lib_dir);
    BARR_mkdir_p(stage_mod_inc_dir);

    char module_build_info[BARR_PATH_MAX];
    snprintf(module_build_info, sizeof(module_build_info), "%s/.barr/data/build_info", module);

    char *mod_name = BARR_get_build_info_key(module_build_info, "name");
    if (mod_name == NULL)
    {
        BARR_errlog("%s(): failed to parse '%s' module name", module, __func__);
        return;
    }

    char *mod_type = BARR_get_build_info_key(module_build_info, "type");
    if (mod_type == NULL)
    {
        BARR_errlog("%s(): failed to parse '%s' module type", module, __func__);
        return;
    }

    if (BARR_strmatch(mod_type, "exec") || BARR_strmatch(mod_type, "executable"))
    {
        char *mod_bdir = BARR_get_build_info_key(module_build_info, "build_dir");
        if (mod_bdir == NULL)
        {
            BARR_errlog("%s(): failed to parse '%s' module build_dir", module, __func__);
            return;
        }
        char bin_dir[BARR_PATH_MAX];
        snprintf(bin_dir, sizeof(bin_dir), "%s/bin", mod_bdir);

        BARR_fs_copy_tree(bin_dir, stage_mod_bin_dir);
    }

    if (BARR_strmatch(mod_type, "shared") || BARR_strmatch(mod_type, "library"))
    {
        char *src_shared = BARR_get_build_info_key(module_build_info, "shared");
        if (src_shared == NULL)
        {
            BARR_errlog("%s(): failed to parse '%s' module shared path", module, __func__);
            return;
        }

        char dst_shared[BARR_PATH_MAX * 4];
        snprintf(dst_shared, sizeof(dst_shared), "%s/%s", stage_mod_lib_dir, basename(src_shared));

        BARR_printf("Copying: %s -> %s\n", src_shared, dst_shared);

        BARR_file_copy(src_shared, dst_shared);
    }

    if (BARR_strmatch(mod_type, "library") || BARR_strmatch(mod_type, "shared") ||
        BARR_strmatch(mod_type, "static"))
    {
        char *cflags_val = BARR_get_build_info_key(module_build_info, "cflags");
        if (cflags_val && cflags_val[0] != '\0')
        {
            const char **tokens = (const char **) BARR_tokenize_string(cflags_val);
            for (const char **p = tokens; p && *p; ++p)
            {
                const char *tok = *p;

                if (strncmp(tok, "-I", 2) == 0)
                {
                    const char *raw_path = tok + 2;
                    char        resolved[BARR_PATH_MAX];

                    if (BARR_path_resolve(module, raw_path, resolved, sizeof(resolved)))
                    {
                        char dst[sizeof(stage_mod_inc_dir) + 32];
                        snprintf(dst, sizeof(dst), "%s/%s", stage_mod_inc_dir, basename(resolved));

                        BARR_printf("DST: %s\n", dst);

                        if (BARR_isdir(raw_path))
                        {
                            BARR_mkdir_p(dst);
                        }

                        if (BARR_fs_copy_tree(resolved, dst) != 0)
                        {
                            BARR_warnlog("Failed to copy include dir: %s", resolved);
                            continue;
                        }
                    }
                    else
                    {
                        BARR_warnlog("Path does not exist, skipping: %s", raw_path);
                    }
                }
            }
        }
    }  // library includes
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
        if (BARR_strmatch(required, "true") || BARR_strmatch(required, "yes") ||
            BARR_strmatch(required, "required"))
        {
            is_required = true;
        }
    }

    char *self = BARR_get_self_exe();

    const char *args[] = {self, "build", "--dir", path, NULL};
    barr_i32    result = BARR_run_process(args[0], (char **) args, false);

    if (result != 0)
    {
        BARR_errlog("%s(): failed to build %s", __func__, path);
        if (is_required)
        {
            BARR_errlog("Module build is required");
            return false;
        }
    }

    barr_module_staging(path);

    // this is for forward observer
    if (!BARR_update_build_info_timestamp(BARR_DATA_BUILD_INFO_PATH))
    {
        BARR_warnlog("%s(): failed to update build info timestamp", __func__);
    }

    if (!barr_add_module_to_registry(name, path))
    {
        BARR_errlog("Failed to register module %s at %s", name, path);
        return false;
    }

    return true;
}
