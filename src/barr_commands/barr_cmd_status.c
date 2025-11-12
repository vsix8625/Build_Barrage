#include "barr_cmd_status.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "olmos_ast.h"
#include "olmos_variables.h"
#include <stdio.h>
#include <string.h>

barr_i32 BARR_cmd_status(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);
    barr_i32 result = 0;

    BARR_printf("--------------------------------------------------------------------------------------\n");

    BARR_InitStatus res = BARR_check_initialized();

    switch (res)
    {
        case BARR_INIT_NOT_FOUND:
        {
            // BARR_is_initialized() already prints logs if you call it
            BARR_init();  // will log errors/warnings
            result = 1;
            break;
        }

        case BARR_INIT_LOCK_NEWER:
        {
            BARR_init();  // logs error about newer lock
            result = 1;
            break;
        }

        case BARR_INIT_LOCK_OLDER:
        {
            BARR_init();  // logs warning
            break;
        }

        case BARR_INIT_OK:
        {
            BARR_log("Build Barrage version %d.%d.%d initialized in %s", BARR_VERSION_MAJOR, BARR_VERSION_MINOR,
                     BARR_VERSION_PATCH, BARR_getcwd());
            break;
        }

        default:
        {
            BARR_errlog("Unknown initialization state!");
            result = 1;
            break;
        }
    }

    //---------------------------------------------------
    // init olmos and parse Barrfile variables

    OLM_init();
    OLM_AST_Node *root = OLM_parse_file("Barrfile");
    if (root == NULL)
    {
        BARR_errlog("%s(): failed to parse Barrfile", __func__);
        return 1;
    }

    OLM_parse_vars(root);

    //---------------------------------------------------

    const char *out_dir_var = OLM_get_var(OLM_VAR_OUT_DIR);

    char obj_dir[BARR_PATH_MAX];
    snprintf(obj_dir, sizeof(obj_dir), "%s/obj", out_dir_var);

    if (out_dir_var == NULL)
    {
        snprintf(obj_dir, sizeof(obj_dir), "build/debug/obj");
    }

    BARR_List obj_list;
    BARR_List src_list;

    BARR_list_init(&obj_list, 64);
    BARR_list_init(&src_list, 64);

    // gen obj list
    BARR_object_files_scan(&obj_list, obj_dir);

    // gen src_list
    FILE *fp = fopen(BARR_DATA_SOURCE_FILES_LOG, "r");
    if (fp == NULL)
    {
        BARR_errlog("Failed to read: %s", BARR_DATA_SOURCE_FILES_LOG);
        return 0;
    }
    else
    {
        char line[BARR_PATH_MAX];
        while (fgets(line, sizeof(line), fp))
        {
            line[strcspn(line, "\r\n")] = 0;
            BARR_list_push(&src_list, BARR_gc_strdup(line));
        }
        fclose(fp);
    }

    BARR_log("Object files status in %s:", obj_dir);

    size_t total = obj_list.count;
    size_t modified = 0;
    size_t unknown = 0;
    size_t up_to_date = 0;

    for (size_t i = 0; i < obj_list.count; i++)
    {
        const char *obj_file = (const char *) obj_list.items[i];
        const char *obj_base = basename(obj_file);

        bool found = false;
        for (size_t j = 0; j < src_list.count; j++)
        {
            const char *src_file = (const char *) src_list.items[j];
            const char *src_base = basename(src_file);

            size_t len = strlen(src_base);
            if (len + 2 == strlen(obj_base) && strncmp(src_base, obj_base, len) == 0)
            {
                found = true;

                bool changed = BARR_is_newer(src_file, obj_file);
                BARR_printf("\t%s\t%s\n", obj_file, changed ? "\033[31;1mmodified" : " ");

                if (changed)
                {
                    modified++;
                }
                else
                {
                    up_to_date++;
                }

                break;
            }
        }

        if (!found)
        {
            BARR_printf("\t%s - unknown\n", obj_file);
            unknown++;
        }
    }

    BARR_printf("--------------------------------------------------------------------------------------\n");
    BARR_printf("[summary]: %zu total, %zu modified, %zu unknown, %zu up-to-date\n", total, modified, unknown,
                up_to_date);
    return result;
}
