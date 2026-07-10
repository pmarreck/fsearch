#include "fsearch_cli.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fsearch_config.h"
#include "fsearch_database.h"
#include "fsearch_database_entry_info.h"
#include "fsearch_database_index_properties.h"
#include "fsearch_database_search_info.h"
#include "fsearch_database_work.h"
#include "fsearch_query.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define FSEARCH_CLI_DEFAULT_RESULT_LIMIT 100
#define FSEARCH_CLI_VIEW_ID 1

typedef struct {
    GMainLoop *loop;
    FsearchDatabase *database;
    FsearchConfig *config;
    const char *search_term;
    guint limit;
    gboolean update_database;
    int exit_code;
} FsearchCliRun;

static gboolean
parse_result_limit(const char *value, guint *limit_out) {
    if (!value || *value == '\0') {
        return FALSE;
    }

    errno = 0;
    char *end = NULL;
    const unsigned long parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > UINT_MAX) {
        return FALSE;
    }

    *limit_out = (guint)parsed;
    return TRUE;
}

/// Selects a frontend without invoking GTK, applying the public environment default before later selector flags.
FsearchCliFrontend
fsearch_cli_select_frontend(guint argc, const char *const argv[], const char *ui_environment) {
    FsearchCliFrontend frontend = FSEARCH_CLI_FRONTEND_GUI;
    if (ui_environment && g_ascii_strcasecmp(ui_environment, "CLI") == 0) {
        frontend = FSEARCH_CLI_FRONTEND_CLI;
    }

    for (guint i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--cli") == 0) {
            frontend = FSEARCH_CLI_FRONTEND_CLI;
        }
        else if (g_strcmp0(argv[i], "--gui") == 0) {
            frontend = FSEARCH_CLI_FRONTEND_GUI;
        }
    }

    return frontend;
}

/// Resolves the bounded output policy with command arguments taking precedence over the environment.
guint
fsearch_cli_resolve_result_limit(const char *environment_limit, const char *argument_limit, gboolean unlimit) {
    if (unlimit) {
        return 0;
    }

    guint limit = FSEARCH_CLI_DEFAULT_RESULT_LIMIT;
    parse_result_limit(environment_limit, &limit);
    parse_result_limit(argument_limit, &limit);
    return limit;
}

/// Renders the cap notice as a deterministic ANSI-styled line for terminal consumers.
char *
fsearch_cli_format_cap_notice(guint limit) {
    return g_strdup_printf("\x1b[3;90mResults capped at %u. Use --unlimit to get them all, or provide a custom --limit, "
                           "or set FSEARCH_RESULT_LIMIT.\x1b[0m",
                           limit);
}

static void
print_help(void) {
    g_print("Usage: fsearch --cli [OPTIONS]\n\n"
            "Search the existing FSearch index and print matching paths.\n\n"
            "  -s, --search PATTERN       Search with FSearch query syntax\n"
            "      --limit COUNT          Print at most COUNT results\n"
            "      --unlimit              Print every result\n"
            "  -u, --update-database      Rescan the configured index and exit\n"
            "      --about                Print one-line build information\n"
            "  -h, --help                 Show this help\n");
}

static const char *
platform_name(void) {
#ifdef G_OS_WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#else
    return "Linux";
#endif
}

static const char *
architecture_name(void) {
#if defined(__aarch64__)
    return "aarch64";
#elif defined(__x86_64__)
    return "x86_64";
#else
    return "unknown-arch";
#endif
}

static void
print_about(void) {
    g_print("FSearch %s - indexed file search for %s %s\n", PACKAGE_VERSION, platform_name(), architecture_name());
}

static void
finish(FsearchCliRun *run, int exit_code) {
    run->exit_code = exit_code;
    g_main_loop_quit(run->loop);
}

static void
on_search_finished(FsearchDatabase *database, FsearchDatabaseSearchInfo *info, gpointer user_data) {
    FsearchCliRun *run = user_data;
    const guint total = fsearch_database_search_info_get_num_entries(info);
    const guint output_count = run->limit == 0 ? total : MIN(total, run->limit);

    for (guint i = 0; i < output_count; i++) {
        g_autoptr(FsearchDatabaseEntryInfo) entry_info = NULL;
        if (fsearch_database_try_get_item_info(database,
                                               FSEARCH_CLI_VIEW_ID,
                                               i,
                                               FSEARCH_DATABASE_ENTRY_INFO_FLAG_PATH_FULL,
                                               &entry_info)
            != FSEARCH_RESULT_SUCCESS) {
            finish(run, EXIT_FAILURE);
            return;
        }

        GString *path = fsearch_database_entry_info_get_path_full(entry_info);
        if (path) {
            g_print("%s\n", path->str);
        }
    }

    if (run->limit > 0 && total > run->limit) {
        g_autofree char *notice = fsearch_cli_format_cap_notice(run->limit);
        g_print("%s\n", notice);
    }

    finish(run, EXIT_SUCCESS);
}

static void
on_database_loaded(FsearchDatabase *database, FsearchDatabaseInfo *info, gpointer user_data) {
    FsearchCliRun *run = user_data;
    if (run->update_database) {
        finish(run, fsearch_database_rescan_blocking(database) == FSEARCH_RESULT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE);
        return;
    }

    g_autoptr(FsearchQuery) query = fsearch_query_new(run->search_term,
                                                       NULL,
                                                       run->config->filters,
                                                       0,
                                                       "cli");
    g_autoptr(FsearchDatabaseWork) search_work = fsearch_database_work_new_search(FSEARCH_CLI_VIEW_ID,
                                                                                    query,
                                                                                    DATABASE_INDEX_PROPERTY_NAME,
                                                                                    GTK_SORT_ASCENDING);
    fsearch_database_queue_work(database, search_work);
}

int
fsearch_cli_run(int argc, char *argv[]) {
    const char *search_term = "";
    const char *argument_limit = NULL;
    gboolean unlimit = FALSE;
    gboolean update_database = FALSE;

    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--cli") == 0) {
            continue;
        }
        if (g_strcmp0(argv[i], "--help") == 0 || g_strcmp0(argv[i], "-h") == 0) {
            print_help();
            return EXIT_SUCCESS;
        }
        if (g_strcmp0(argv[i], "--about") == 0) {
            print_about();
            return EXIT_SUCCESS;
        }
        if (g_strcmp0(argv[i], "--unlimit") == 0) {
            unlimit = TRUE;
            argument_limit = NULL;
            continue;
        }
        if (g_strcmp0(argv[i], "--limit") == 0 && i + 1 < argc) {
            argument_limit = argv[++i];
            unlimit = FALSE;
            continue;
        }
        if ((g_strcmp0(argv[i], "--search") == 0 || g_strcmp0(argv[i], "-s") == 0) && i + 1 < argc) {
            search_term = argv[++i];
            continue;
        }
        if (g_strcmp0(argv[i], "--update-database") == 0 || g_strcmp0(argv[i], "-u") == 0) {
            update_database = TRUE;
            continue;
        }

        g_printerr("fsearch: unknown CLI option: %s\n", argv[i]);
        return EXIT_FAILURE;
    }

    FsearchCliRun run = {
        .loop = g_main_loop_new(NULL, FALSE),
        .config = g_new0(FsearchConfig, 1),
        .search_term = search_term,
        .limit = fsearch_cli_resolve_result_limit(g_getenv("FSEARCH_RESULT_LIMIT"), argument_limit, unlimit),
        .update_database = update_database,
        .exit_code = EXIT_FAILURE,
    };
    if (!config_load(run.config)) {
        config_load_default(run.config);
    }

    g_autofree char *database_path = g_build_filename(g_get_user_data_dir(), "fsearch", "fsearch.db", NULL);
    g_autoptr(GFile) database_file = g_file_new_for_path(database_path);
    run.database = fsearch_database_new(g_steal_pointer(&database_file), run.config->includes, run.config->excludes);
    g_signal_connect(run.database, "load-finished", G_CALLBACK(on_database_loaded), &run);
    g_signal_connect(run.database, "search-finished", G_CALLBACK(on_search_finished), &run);

    g_autoptr(FsearchDatabaseWork) load_work = fsearch_database_work_new_load();
    fsearch_database_queue_work(run.database, load_work);
    g_main_loop_run(run.loop);

    g_clear_object(&run.database);
    config_free(run.config);
    g_main_loop_unref(run.loop);
    return run.exit_code;
}
