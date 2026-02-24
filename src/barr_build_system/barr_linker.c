#include "barr_linker.h"
#include "barr_debug.h"
#include "barr_env.h"
#include "barr_find_package.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "barr_modules.h"
#include "olmos.h"
#include "olmos_variables.h"

#include <stdlib.h>
#include <string.h>

barr_i32 BARR_archive_stage(BARR_SourceList *list, const char *archive_path)
{
    if (list == NULL)
    {
        BARR_errlog("%s(): list is NULL", __func__);
        return 1;
    }
    if (archive_path == NULL)
    {
        BARR_errlog("%s(): path is NULL", __func__);
        return 1;
    }

    size_t argc = list->count + 3;
    char **args = BARR_gc_alloc(sizeof(char *) * (argc + 1));
    size_t idx  = 0;
    args[idx++] = "ar";
    args[idx++] = "rcs";
    args[idx++] = (char *) archive_path;

    for (size_t i = 0; i < list->count; ++i)
    {
        args[idx++] = list->entries[i];
    }
    args[idx] = NULL;

    barr_i32 ret = BARR_run_process(args[0], args, false);
    return ret;
}

barr_i32 BARR_link_stage(char **link_args)
{
    return BARR_run_process(link_args[0], link_args, false);
}

// ----------------------------------------------------------------------------------------------------

BARR_LinkArgs *BARR_link_args_create(void)
{
    BARR_LinkArgs *la = BARR_gc_alloc(sizeof(BARR_LinkArgs));
    if (la == NULL)
    {
        return NULL;
    }
    la->capacity = BARR_INITIAL_LINK_ARGS_CAPACITY;
    la->count    = 0;
    la->args     = BARR_gc_alloc(sizeof(char *) * la->capacity);
    la->args[0]  = NULL;
    return la;
}

void BARR_link_args_add(BARR_LinkArgs *la, const char *arg)
{
    if (!la || !arg)
    {
        return;
    }

    if (la->count >= la->capacity)
    {
        size_t new_capacity = (la->capacity == 0) ? 16 : la->capacity * 2;
        char **new_args     = BARR_gc_realloc(la->args, new_capacity * sizeof(char *));
        if (new_args == NULL)
        {
            BARR_errlog("%s(): failed to realloc linker args", __func__);
            return;
        }

        la->args     = new_args;
        la->capacity = new_capacity;
    }

    la->args[la->count] = BARR_gc_strdup(arg);
    la->count++;
    la->args[la->count] = NULL;
}

char **BARR_link_args_finalize(BARR_LinkArgs *la)
{
    if (la == NULL)
    {
        return NULL;
    }
    la->args[la->count] = NULL;
    return la->args;
}

void BARR_dedup_libs_and_add_to_link_args(BARR_LinkArgs *la, const char **libs_array)
{
    if (!la || !libs_array)
    {
        return;
    }

    const char **deduped = BARR_dedup_flags_array(libs_array);
    if (!deduped)
    {
        return;
    }

    for (size_t i = 0; deduped[i] != NULL; ++i)
    {
        BARR_link_args_add(la, deduped[i]);
    }
}

void BARR_link_collect_pkg_list(BARR_List *list, BARR_LinkArgs *input, BARR_LinkArgs *flags)
{
    if (list == NULL || input == NULL || flags == NULL)
    {
        return;
    }

    for (size_t i = 0; i < list->count; i++)
    {
        const char *entry = list->items[i];
        if (entry == NULL)
        {
            continue;
        }

        char **tok = BARR_tokenize_string(entry);
        if (tok == NULL)
        {
            continue;
        }

        for (char **t = tok; *t; t++)
        {
            const char *s = *t;

            if (strncmp(s, "-L", 2) == 0 || strncmp(s, "-l", 2) == 0)
            {
                BARR_link_args_add(input, s);
            }
            else
            {
                BARR_link_args_add(flags, s);
            }
        }
    }
}

barr_i32 BARR_link_target(const char      *target_type,
                          const char      *target_name,
                          const char      *out_dir,
                          BARR_SourceList *object_list,
                          BARR_List       *pkg_list,
                          size_t           n_threads,
                          const char      *resolved_compiler,
                          const char      *linker,
                          const char      *module_includes,
                          const char      *target_version,
                          const char      *main_source)
{
    struct timespec link_start, link_end;
    clock_gettime(CLOCK_MONOTONIC, &link_start);

    if (target_name == NULL || target_type == NULL || out_dir == NULL || object_list == NULL ||
        pkg_list == NULL || resolved_compiler == NULL)
    {
        BARR_errlog("%s(): NULL argument", __func__);
        return 1;
    }

    n_threads = n_threads != 0 ? n_threads : 1;
    if ((barr_i32) n_threads > BARR_OS_GET_CORES())
    {
        n_threads = BARR_OS_GET_CORES();
    }

    // ----------------------------------------------------------------------------
    // archive stage (always create libbarr.a)
    // ----------------------------------------------------------------------------
    char archive_out_path[BARR_PATH_MAX * 2];
    snprintf(archive_out_path, sizeof(archive_out_path), "%s/libbarr.a", out_dir);
    BARR_dbglog("Archive path: %s", archive_out_path);

    barr_i32 archive_ret = BARR_archive_stage(object_list, archive_out_path);
    if (archive_ret != 0)
    {
        BARR_errlog("Archive stage failed");
        return archive_ret;
    }

    // ----------------------------------------------------------------------------
    // Prepare link args
    // ----------------------------------------------------------------------------

    BARR_LinkArgs *la = BARR_link_args_create();

    if (la == NULL)
    {
        BARR_errlog("Failed to create link args");
        return 1;
    }

    BARR_link_args_add(la, resolved_compiler);

    if (BARR_strmatch(target_type, "executable") || BARR_strmatch(target_type, "exec"))
    {
        char main_co_buf[BARR_PATH_MAX * 2];
        snprintf(main_co_buf, sizeof(main_co_buf), "%s/obj/%s.o", out_dir, main_source);
        BARR_link_args_add(la, main_co_buf);
    }

    if (linker && linker[0])
    {
        char fuse_ld[BARR_BUF_SIZE_32];
        snprintf(fuse_ld, sizeof(fuse_ld), "-fuse-ld=%s", linker);
        BARR_link_args_add(la, fuse_ld);

        if (BARR_strmatch(linker, "lld"))
        {
            char lld_threads[BARR_BUF_SIZE_16];
            snprintf(lld_threads, sizeof(lld_threads), "-Wl,--threads=%zu", n_threads);
            BARR_link_args_add(la, lld_threads);
        }
    }

    for (size_t i = 0; i < object_list->count; ++i)
    {
        BARR_link_args_add(la, object_list->entries[i]);
    }

    // archive lib dir path
    char archive_lib_dirpath[BARR_PATH_MAX * 2];
    snprintf(archive_lib_dirpath, sizeof(archive_lib_dirpath), "-L%s", out_dir);
    BARR_link_args_add(la, archive_lib_dirpath);

    const char *user_lflags = OLM_get_var(OLM_VAR_LINKER_FLAGS);
    if (user_lflags && user_lflags[0])
    {
        char **tokens = BARR_tokenize_string(user_lflags);
        for (char **t = tokens; t && *t; ++t)
        {
            BARR_link_args_add(la, *t);
        }
    }

    const char *user_lib_paths = OLM_get_var(OLM_VAR_LIB_PATHS);
    if (user_lib_paths && user_lib_paths[0])
    {
        char **tokens = BARR_tokenize_string(user_lib_paths);
        for (char **t = tokens; t && *t; ++t)
        {
            BARR_link_args_add(la, *t);
        }
    }

    // packages and modules
    for (size_t i = 0; i < BARR_get_module_count(); ++i)
    {
        BARR_Module *mod = &BARR_get_module_array()[i];
        char         build_info_path[BARR_PATH_MAX];
        snprintf(build_info_path,
                 sizeof(build_info_path),
                 "%s/%s",
                 mod->path,
                 BARR_DATA_BUILD_INFO_PATH);

        if (!BARR_isfile(build_info_path))
        {
            if (g_barr_verbose)
            {
                BARR_warnlog("Module '%s' has no build.info, skipping", mod->name);
            }
            continue;
        }

        char *name     = BARR_get_build_info_key(build_info_path, "name");
        char *type     = BARR_get_build_info_key(build_info_path, "type");
        char *lib_path = BARR_get_build_info_key(build_info_path, "libpath");
        char *runtime  = BARR_get_build_info_key(build_info_path, "rpath");

        if (type == NULL)
        {
            type = "library";
        }

        if (BARR_strmatch(type, "static") || BARR_strmatch(type, "library"))
        {
            if (lib_path && lib_path[0])
            {
                BARR_link_args_add(la, lib_path);
            }
            char name_arg[BARR_PATH_MAX];
            snprintf(name_arg, sizeof(name_arg), "-l%s", name);
            BARR_link_args_add(la, name_arg);
        }

        if (BARR_strmatch(type, "shared") || BARR_strmatch(type, "library"))
        {
            if (lib_path && lib_path[0])
            {
                BARR_link_args_add(la, lib_path);
            }
            char name_arg[BARR_PATH_MAX];
            snprintf(name_arg, sizeof(name_arg), "-l%s", name);
            BARR_link_args_add(la, name_arg);

            if (runtime && runtime[0])
            {
                BARR_link_args_add(la, runtime);
            }
        }
    }

    // packages
    for (size_t i = 0; i < pkg_list->count; ++i)
    {
        BARR_PackageInfo *pkg_info = (BARR_PackageInfo *) pkg_list->items[i];
        if (pkg_info == NULL || pkg_info->libs == NULL || !pkg_info->libs[0])
        {
            continue;
        }

        char **tokens = BARR_tokenize_string(pkg_info->libs);
        for (size_t i = 0; tokens && tokens[i]; ++i)
        {
            char *t = tokens[i];
            if (t[0] == '-' && (t[1] == 'L' || t[1] == 'l'))
            {
                BARR_link_args_add(la, t);
            }
            else
            {
                BARR_link_args_add(la, t);
            }
        }
    }

    // always link libbarr
    BARR_link_args_add(la, "-lbarr");

    // ----------------------------------------------------------------------------
    // Target-specific link logic
    // ----------------------------------------------------------------------------

    // compute directories based on target
    char lib_dir[BARR_PATH_MAX + BARR_BUF_SIZE_256];
    char bin_dir[BARR_PATH_MAX + BARR_BUF_SIZE_256];

    snprintf(lib_dir, sizeof(lib_dir), "%s/lib", out_dir);
    snprintf(bin_dir, sizeof(bin_dir), "%s/bin", out_dir);

    if (BARR_strmatch(target_type, "static") || BARR_strmatch(target_type, "library") ||
        BARR_strmatch(target_type, "shared"))
    {
        BARR_mkdir_p(lib_dir);
    }

    if (BARR_strmatch(target_type, "executable") || BARR_strmatch(target_type, "exec"))
    {
        BARR_mkdir_p(bin_dir);

        char exe_path[BARR_PATH_MAX * 2];
        snprintf(exe_path, sizeof(exe_path), "%s/%s", bin_dir, target_name);

        BARR_link_args_add(la, "-o");
        BARR_link_args_add(la, exe_path);

        char **link_args = BARR_link_args_finalize(la);

        if (g_barr_verbose)
        {
            for (char **f = link_args; f && *f; ++f)
            {
                BARR_printf("%s ", *f);
            }
            BARR_printf("\n ");
        }

        barr_i32 ret = BARR_link_stage(link_args);
        if (ret != 0)
        {
            BARR_errlog("Executable link stage  failed");
            return 1;
        }

        char        build_info_contents[BARR_BUF_SIZE_8192 * 5];
        const char *barr_ver = BARR_version_get_str();
        snprintf(build_info_contents,
                 sizeof(build_info_contents),
                 "[common]\n"
                 "name = %s\n"
                 "type = %s\n"
                 "version = %s\n"
                 "barr_version = %s\n"
                 "timestamp = %ld\n"
                 "\n[paths]\n"
                 "build_dir = %s\n",
                 target_name,
                 target_type,
                 target_version,
                 barr_ver,
                 time(NULL),
                 out_dir);

        BARR_file_write(BARR_DATA_BUILD_INFO_PATH, "%s", build_info_contents);
    }
    else
    {
        char dest_static[BARR_PATH_MAX * 2];
        if (BARR_strmatch(target_type, "static") || BARR_strmatch(target_type, "library"))
        {
            snprintf(dest_static, sizeof(dest_static), "%s/lib%s.a", lib_dir, target_name);
            BARR_mv(archive_out_path, dest_static);
        }

        // SHARED
        char dest_shared[BARR_PATH_MAX * 2];
        if (BARR_strmatch(target_type, "shared") || BARR_strmatch(target_type, "library"))
        {
            snprintf(dest_shared,
                     sizeof(dest_shared),
                     "%s/lib%s.%s",
                     lib_dir,
                     target_name,
                     BARR_DYNAMIC_LIB_EXT);

            BARR_link_args_add(la, "-shared");
            BARR_link_args_add(la, "-fPIC");
            BARR_link_args_add(la, "-o");
            BARR_link_args_add(la, dest_shared);

            if (g_barr_verbose)
            {
                BARR_link_args_debug(la);
            }
            char   **link_args = BARR_link_args_finalize(la);
            barr_i32 ret       = BARR_link_stage(link_args);
            if (ret != 0)
            {
                BARR_errlog("Shared lib creation failed");
                return 1;
            }
        }

        char lib_dir_final[BARR_PATH_MAX];
        snprintf(lib_dir_final, sizeof(lib_dir_final), "%s/lib", out_dir);

        char        build_info_contents[BARR_BUF_SIZE_32K * 2];
        const char *barr_ver = BARR_version_get_str();

        snprintf(build_info_contents,
                 sizeof(build_info_contents),
                 "[common]\n"
                 "name = %s\n"
                 "type = %s\n"
                 "version = %s\n"
                 "barr_version = %s\n"
                 "timestamp = %ld\n"
                 "\n[paths]\n"
                 "cflags = %s\n"
                 "libpath = -L%s\n"
                 "rpath = -Wl,-rpath,%s\n"
                 "\n[artifacts]\n"
                 "static = %s\n"
                 "shared = %s\n",
                 target_name,
                 target_type,
                 target_version,
                 barr_ver,
                 time(NULL),
                 module_includes,
                 lib_dir_final,
                 lib_dir_final,
                 BARR_strmatch(target_type, "shared") ? "" : dest_static,
                 BARR_strmatch(target_type, "static") ? "" : dest_shared);

        BARR_file_write(BARR_DATA_BUILD_INFO_PATH, "%s", build_info_contents);
    }

    char origin[BARR_PATH_MAX];
    snprintf(origin, sizeof(origin), "-Wl,-rpath,$ORIGIN/../share/%s/lib", target_name);
    BARR_link_args_add(la, origin);

    clock_gettime(CLOCK_MONOTONIC, &link_end);
    BARR_log("Link time: \033[34;1m %s\033[0m", BARR_fmt_time_elapsed(&link_start, &link_end));
    return 0;
}

void BARR_link_args_debug(const BARR_LinkArgs *la)
{
    if (!la)
    {
        BARR_errlog("%s(): NULL link args", __func__);
        return;
    }

    BARR_printf("Link args (count = %zu):\n", la->count);
    for (size_t i = 0; i < la->count; ++i)
    {
        BARR_printf("[%zu]: '%s'\n", i, la->args[i]);
    }
}
