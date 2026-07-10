#include "fsearch_cli.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fsearch_config.h"
#include "fsearch_database.h"
#include "fsearch_database_entry_info.h"
#include "fsearch_database_file.h"
#include "fsearch_database_index_properties.h"
#include "fsearch_database_search_info.h"
#include "fsearch_database_work.h"
#include "fsearch_query.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef G_OS_WIN32
#include <io.h>
#define fsearch_cli_isatty _isatty
#define fsearch_cli_fileno _fileno
#else
#include <glib-unix.h>
#include <signal.h>
#include <unistd.h>
#define fsearch_cli_isatty isatty
#define fsearch_cli_fileno fileno
#endif

#define FSEARCH_CLI_DEFAULT_RESULT_LIMIT 100
#define FSEARCH_CLI_VIEW_ID 1
#define FSEARCH_CLI_ITALIC_LIGHT_GREY "\x1b[3;37m"
#define FSEARCH_CLI_ANSI_RESET "\x1b[0m"

typedef struct {
    GMainLoop *loop;
    FsearchDatabase *database;
    FsearchDatabaseWork *search_work;
    FsearchConfig *config;
    const char *search_term;
    guint limit;
    gboolean update_database;
    gboolean wait_for_index_update;
    gboolean search_queued;
    gboolean index_update_notice_printed;
    gboolean index_progress_visible;
    guint interrupt_source;
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
    return g_strdup_printf(FSEARCH_CLI_ITALIC_LIGHT_GREY
                           "Results capped at %u. Use --unlimit or --unlimited to get them all, or provide a custom --limit, "
                           "or set FSEARCH_RESULT_LIMIT."
                           FSEARCH_CLI_ANSI_RESET,
                           limit);
}

/// Renders the complete result count as a muted status line for stderr.
char *
fsearch_cli_format_result_count_notice(guint total) {
    return g_strdup_printf(FSEARCH_CLI_ITALIC_LIGHT_GREY "%u match%s." FSEARCH_CLI_ANSI_RESET,
                           total,
                           total == 1 ? "" : "es");
}

/// Renders the index-rebuild status line for stderr without mixing it into path output.
char *
fsearch_cli_format_index_update_notice(void) {
    return g_strdup(FSEARCH_CLI_ITALIC_LIGHT_GREY "Updating FSearch index before searching..." FSEARCH_CLI_ANSI_RESET);
}

/// Renders an in-place, indeterminate scan status using observed entries rather than a fabricated percentage.
char *
fsearch_cli_format_index_progress(const char *status) {
    g_return_val_if_fail(status != NULL, NULL);
    return g_strdup_printf("\r\x1b[2K" FSEARCH_CLI_ITALIC_LIGHT_GREY
                           "Indexing: %s"
                           FSEARCH_CLI_ANSI_RESET,
                           status);
}

/// Builds a path-constrained query while escaping the path as one literal FSearch term.
char *
fsearch_cli_build_scoped_query(const char *query, const char *scope_path, gboolean global) {
    g_return_val_if_fail(query != NULL, NULL);
    if (global) {
        return g_strdup(query);
    }

    g_return_val_if_fail(scope_path != NULL, NULL);
    g_autoptr(GString) scoped = g_string_new("path:\"");
    for (const char *c = scope_path; *c; c++) {
        if (*c == '\\' || *c == '\"') {
            g_string_append_c(scoped, '\\');
        }
        g_string_append_c(scoped, *c);
    }
    if (!g_str_has_suffix(scope_path, G_DIR_SEPARATOR_S)) {
        g_string_append_c(scoped, G_DIR_SEPARATOR);
    }
    g_string_append_c(scoped, '\"');
    if (*query != '\0') {
        g_string_append_printf(scoped, " AND (%s)", query);
    }
    return g_string_free(g_steal_pointer(&scoped), FALSE);
}

static void
print_help(void) {
    g_print("Usage: fsearch --cli [OPTIONS]\n\n"
            "Search the existing FSearch index and print matching paths.\n\n"
            "  -s, --search PATTERN       Search with FSearch query syntax\n"
            "      --path DIRECTORY        Search recursively below DIRECTORY (default: current directory)\n"
            "      --global                Search every indexed location\n"
            "      --limit COUNT          Print at most COUNT results\n"
            "      --unlimit, --unlimited Print every result\n"
            "  -u, --update-database      Rescan the configured index and exit\n"
            "      --about                Print one-line build information\n"
            "  -h, --help                 Show this help\n\n"
            "Examples:\n"
            "  fsearch --cli --search invoice              # current directory\n"
            "  fsearch --cli --path ~/Documents --search invoice\n"
            "  fsearch --cli --global --search invoice\n"
            "  fsearch --cli --search 'file:size:>20mb'\n"
            "  fsearch --cli --search 'ext:jpg;png'\n"
            "  fsearch --cli --search 'path:projects report'\n"
            "  fsearch --cli --search 'report AND ext:pdf'\n"
            "  fsearch --cli --search '(draft OR final) !archive'\n"
            "  fsearch --cli --search 'file:regex:\".+\\.pdf$\"'\n"
            "  fsearch --cli --search 'case:AGENTS'        # case-sensitive\n"
            "  fsearch --cli --search 'case:regex:\"^AGENTS\\.md$\"'\n"
            "  fsearch --cli --search 'glob:**/*.png'\n"
            "  fsearch --cli --search 'path:glob:**/AGENTS.md'\n"
            "  fsearch --cli --search 'case:glob:AGENTS.md' # case-sensitive glob\n\n"
            "Examples use space as AND. Try file:, folder:, path:, ext:, size:, dm:, case:, regex:, and glob:.\n"
            "Boolean syntax: AND or &&; OR or ||; NOT or !. Parentheses group terms.\n"
            "Glob syntax: * excludes /; ** spans directories; ? matches one character; [a-c] is a range;\n"
            "Glob searches paths by default; use **/ for any depth; file:glob: searches file names.\n"
            "{png,jpg} selects alternatives; {01..12} is an inclusive numeric range; \\* matches a literal *.\n");
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

static gboolean
stderr_is_terminal(void) {
    return fsearch_cli_isatty(fsearch_cli_fileno(stderr)) != 0;
}

static gboolean
database_requires_index_update(const char *database_path, FsearchConfig *config) {
    g_autoptr(FsearchDatabaseIncludeManager) indexed_includes = NULL;
    g_autoptr(FsearchDatabaseExcludeManager) indexed_excludes = NULL;
    FsearchDatabaseIndexPropertyFlags flags = DATABASE_INDEX_PROPERTY_FLAG_NONE;

    if (!fsearch_database_file_load_config(database_path, &indexed_includes, &indexed_excludes, &flags)) {
        return TRUE;
    }

    return !fsearch_database_include_manager_equal(indexed_includes, config->includes)
        || !fsearch_database_exclude_manager_equal(indexed_excludes, config->excludes);
}

static void
queue_search(FsearchCliRun *run) {
    if (run->search_queued) {
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
    run->search_queued = TRUE;
    g_clear_pointer(&run->search_work, fsearch_database_work_unref);
    run->search_work = fsearch_database_work_ref(search_work);
    fsearch_database_queue_work(run->database, search_work);
}

#ifndef G_OS_WIN32
static gboolean
on_interrupt(gpointer user_data) {
    FsearchCliRun *run = user_data;
    run->interrupt_source = 0;

    if (run->index_progress_visible) {
        g_printerr("\r\x1b[2K");
        run->index_progress_visible = FALSE;
    }
    if (run->search_work) {
        fsearch_database_work_cancel(run->search_work);
    }
    fsearch_database_cancel(run->database);
    g_printerr("fsearch: interrupted\n");
    finish(run, 130);
    return G_SOURCE_REMOVE;
}
#endif

static void
on_search_finished(FsearchDatabase *database, guint id, FsearchDatabaseSearchInfo *info, gpointer user_data) {
    FsearchCliRun *run = user_data;
    if (id != FSEARCH_CLI_VIEW_ID) {
        return;
    }
    g_clear_pointer(&run->search_work, fsearch_database_work_unref);
    if (!info) {
        g_printerr("fsearch: search could not run because the index is unavailable\n");
        finish(run, EXIT_FAILURE);
        return;
    }
    const guint total = fsearch_database_search_info_get_num_entries(info);
    const guint output_count = run->limit == 0 ? total : MIN(total, run->limit);

    if (run->index_progress_visible) {
        g_printerr("\r\x1b[2K");
        run->index_progress_visible = FALSE;
    }

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
    else {
        fflush(stdout);
        g_autofree char *notice = fsearch_cli_format_result_count_notice(total);
        g_printerr("%s\n", notice);
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

    if (!run->wait_for_index_update) {
        queue_search(run);
    }
}

static void
on_database_scan_started(FsearchDatabase *database, gpointer user_data) {
    FsearchCliRun *run = user_data;
    if (run->update_database || run->index_update_notice_printed) {
        return;
    }

    run->wait_for_index_update = TRUE;
    run->index_update_notice_printed = TRUE;
    g_autofree char *notice = fsearch_cli_format_index_update_notice();
    g_printerr("%s\n", notice);
}

static void
on_database_scan_finished(FsearchDatabase *database, FsearchDatabaseInfo *info, gpointer user_data) {
    FsearchCliRun *run = user_data;
    if (!run->update_database && run->wait_for_index_update) {
        queue_search(run);
    }
}

static void
on_database_progress(FsearchDatabase *database, char *text, gpointer user_data) {
    FsearchCliRun *run = user_data;
    if (run->update_database || !run->wait_for_index_update || !stderr_is_terminal()) {
        return;
    }

    g_autofree char *progress = fsearch_cli_format_index_progress(text);
    g_printerr("%s", progress);
    run->index_progress_visible = TRUE;
}

int
fsearch_cli_run(int argc, char *argv[]) {
    const char *search_term = "";
    const char *argument_limit = NULL;
    const char *argument_path = NULL;
    gboolean unlimit = FALSE;
    gboolean global = FALSE;
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
        if (g_strcmp0(argv[i], "--version") == 0 || g_strcmp0(argv[i], "-v") == 0) {
            g_print("FSearch %s\n", PACKAGE_VERSION);
            return EXIT_SUCCESS;
        }
        if (g_strcmp0(argv[i], "--unlimit") == 0 || g_strcmp0(argv[i], "--unlimited") == 0) {
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
        if (g_strcmp0(argv[i], "--path") == 0 && i + 1 < argc) {
            argument_path = argv[++i];
            global = FALSE;
            continue;
        }
        if (g_strcmp0(argv[i], "--global") == 0) {
            argument_path = NULL;
            global = TRUE;
            continue;
        }
        if (g_strcmp0(argv[i], "--update-database") == 0 || g_strcmp0(argv[i], "-u") == 0) {
            update_database = TRUE;
            continue;
        }

        g_printerr("fsearch: unknown CLI option: %s\n", argv[i]);
        return EXIT_FAILURE;
    }

    g_autofree char *scope_path = global ? NULL : (argument_path ? g_canonicalize_filename(argument_path, NULL)
                                                                    : g_get_current_dir());
    g_autofree char *scoped_query = fsearch_cli_build_scoped_query(search_term, scope_path, global);

    FsearchCliRun run = {
        .loop = g_main_loop_new(NULL, FALSE),
        .config = g_new0(FsearchConfig, 1),
        .search_term = scoped_query,
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
    run.wait_for_index_update = database_requires_index_update(database_path, run.config);
    g_signal_connect(run.database, "load-finished", G_CALLBACK(on_database_loaded), &run);
    g_signal_connect(run.database, "scan-started", G_CALLBACK(on_database_scan_started), &run);
    g_signal_connect(run.database, "scan-finished", G_CALLBACK(on_database_scan_finished), &run);
    g_signal_connect(run.database, "database-progress", G_CALLBACK(on_database_progress), &run);
    g_signal_connect(run.database, "search-finished", G_CALLBACK(on_search_finished), &run);

#ifndef G_OS_WIN32
    run.interrupt_source = g_unix_signal_add(SIGINT, on_interrupt, &run);
#endif

    g_autoptr(FsearchDatabaseWork) load_work = fsearch_database_work_new_load();
    fsearch_database_queue_work(run.database, load_work);
    g_main_loop_run(run.loop);

    if (run.interrupt_source != 0) {
        g_source_remove(run.interrupt_source);
    }
    g_clear_pointer(&run.search_work, fsearch_database_work_unref);
    g_clear_object(&run.database);
    config_free(run.config);
    g_main_loop_unref(run.loop);
    return run.exit_code;
}
