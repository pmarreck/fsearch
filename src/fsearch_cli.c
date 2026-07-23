#include "fsearch_cli.h"
#include "fsearch_cli_config.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fsearch_config.h"
#include "fsearch_database.h"
#include "fsearch_database_entry_info.h"
#include "fsearch_database_file.h"
#include "fsearch_database_index_properties.h"
#include "fsearch_database_refresh_policy.h"
#include "fsearch_database_search_info.h"
#include "fsearch_database_work.h"
#include "fsearch_query.h"

#include <glib/gi18n.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
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
    FsearchDatabaseRefreshAction refresh_action;
    gboolean saving_index;
    gboolean search_queued;
    gboolean index_update_notice_printed;
    gboolean index_progress_visible;
    guint interrupt_source;
    int exit_code;
} FsearchCliRun;

/// Localizes CLI diagnostics before preserving GLib's stderr-only output contract.
void
fsearch_cli_printerr(const char *format, ...) {
    va_list args;
    va_start(args, format);
    g_autofree char *message = g_strdup_vprintf(_(format), args);
    va_end(args);
    g_printerr("%s", message);
}

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

static gboolean
parse_refresh_interval(const char *value, gint64 *interval_out) {
    if (!value || *value == '\0') {
        return FALSE;
    }

    errno = 0;
    char *end = NULL;
    const gint64 interval = g_ascii_strtoll(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || interval < 0) {
        return FALSE;
    }

    *interval_out = interval;
    return TRUE;
}

/// Selects a frontend without invoking GTK, applying the public environment default before later selector flags.
FsearchCliFrontend
fsearch_cli_select_frontend(guint argc, const char *const argv[], const char *ui_environment) {
    FsearchCliFrontend frontend = FSEARCH_CLI_FRONTEND_GUI;
    if (ui_environment && g_ascii_strcasecmp(ui_environment, "CLI") == 0) {
        frontend = FSEARCH_CLI_FRONTEND_CLI;
    }
    if (argc > 1 && g_strcmp0(argv[1], "config") == 0) {
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
    g_autofree char *message = g_strdup_printf(_("Results capped at %u. Use --unlimit or --unlimited to get them all, "
                                                  "or provide a custom --limit, or set FSEARCH_RESULT_LIMIT."),
                                               limit);
    return g_strdup_printf(FSEARCH_CLI_ITALIC_LIGHT_GREY "%s" FSEARCH_CLI_ANSI_RESET, message);
}

/// Renders the complete result count as a muted status line for stderr.
char *
fsearch_cli_format_result_count_notice(guint total) {
    g_autofree char *message = g_strdup_printf(ngettext("%u match.", "%u matches.", total), total);
    return g_strdup_printf(FSEARCH_CLI_ITALIC_LIGHT_GREY "%s" FSEARCH_CLI_ANSI_RESET, message);
}

/// Renders the index-rebuild status line for stderr without mixing it into path output.
char *
fsearch_cli_format_index_update_notice(void) {
    return g_strdup_printf(FSEARCH_CLI_ITALIC_LIGHT_GREY "%s" FSEARCH_CLI_ANSI_RESET,
                           _("Updating FSearch index before searching..."));
}

/// Renders the explicit stale-index status without mixing a warning into searchable path output.
char *
fsearch_cli_format_stale_index_notice(void) {
    return g_strdup_printf(FSEARCH_CLI_ITALIC_LIGHT_GREY "%s" FSEARCH_CLI_ANSI_RESET,
                           _("Using stale FSearch index; automatic reindexing is disabled."));
}

/// Renders an in-place, indeterminate scan status using observed entries rather than a fabricated percentage.
char *
fsearch_cli_format_index_progress(const char *status) {
    g_return_val_if_fail(status != NULL, NULL);
    g_autofree char *message = g_strdup_printf(_("Indexing: %s"), status);
    return g_strdup_printf("\r\x1b[2K" FSEARCH_CLI_ITALIC_LIGHT_GREY "%s" FSEARCH_CLI_ANSI_RESET, message);
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
    g_print("%s", _("Usage: fsearch --cli [OPTIONS]\n\n"
            "Search the existing FSearch index and print matching paths.\n\n"
            "  -s, --search PATTERN       Search with FSearch query syntax\n"
            "      --path DIRECTORY        Search recursively below DIRECTORY (default: current directory)\n"
            "      --global                Search every indexed location\n"
            "      --limit COUNT          Print at most COUNT results\n"
            "      --unlimit, --unlimited Print every result\n"
            "  -u, --update-database      Rescan the configured index and exit\n"
            "      --about                Print one-line build information\n"
            "  -h, --help                 Show this help\n\n"
            "Configuration: fsearch config --help\n\n"
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
            "{png,jpg} selects alternatives; {01..12} is an inclusive numeric range; \\* matches a literal *.\n"));
    g_print("%s", _("\nIndex refresh options:\n"
                    "  --refresh, --refresh-index    Rescan before searching\n"
                    "  --no-reindex                  Use the existing index without rescanning\n"
                    "  --reindex-interval SECONDS    Refresh when the index is this old\n"));
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
    g_print(_("FSearch %s - indexed file search for %s %s\n"), PACKAGE_VERSION, platform_name(), architecture_name());
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

static void
queue_search(FsearchCliRun *run) {
    if (run->search_queued) {
        return;
    }

    g_autoptr(FsearchQuery) query = fsearch_query_new(run->search_term,
                                                       NULL,
                                                       run->config->filters,
                                                       config_get_search_query_flags(run->config),
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

static void
queue_refresh(FsearchCliRun *run) {
    g_autoptr(FsearchDatabaseWork) refresh_work = fsearch_database_work_new_rescan();
    fsearch_database_queue_work(run->database, refresh_work);
}

static void
queue_index_save(FsearchCliRun *run) {
    g_autoptr(FsearchDatabaseWork) save_work = fsearch_database_work_new_save();
    fsearch_database_queue_work(run->database, save_work);
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
    fsearch_cli_printerr("fsearch: interrupted\n");
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
        fsearch_cli_printerr("fsearch: search could not run because the index is unavailable\n");
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
        fsearch_cli_printerr("%s\n", notice);
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

    if (run->refresh_action == FSEARCH_DATABASE_REFRESH_ACTION_REFRESH) {
        queue_refresh(run);
        return;
    }
    if (run->refresh_action == FSEARCH_DATABASE_REFRESH_ACTION_USE_STALE) {
        g_autofree char *notice = fsearch_cli_format_stale_index_notice();
        fsearch_cli_printerr("%s\n", notice);
    }
    queue_search(run);
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
    fsearch_cli_printerr("%s\n", notice);
}

static void
on_database_scan_finished(FsearchDatabase *database, FsearchDatabaseInfo *info, gpointer user_data) {
    FsearchCliRun *run = user_data;
    if (!run->update_database && run->wait_for_index_update) {
        run->saving_index = TRUE;
        queue_index_save(run);
    }
}

static void
on_database_save_finished(FsearchDatabase *database, gpointer user_data) {
    FsearchCliRun *run = user_data;
    if (!run->update_database && run->saving_index) {
        run->saving_index = FALSE;
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
    fsearch_cli_printerr("%s", progress);
    run->index_progress_visible = TRUE;
}

int
fsearch_cli_run(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--cli") == 0) {
            continue;
        }
        if (g_strcmp0(argv[i], "config") == 0) {
            const int config_result = fsearch_cli_config_run(argc - i, argv + i);
            if (config_result != FSEARCH_CLI_CONFIG_RESCAN_REQUESTED) {
                return config_result;
            }
            char *update_argv[] = {argv[0], "--cli", "--update-database", NULL};
            return fsearch_cli_run(3, update_argv);
        }
        break;
    }

    const char *search_term = "";
    const char *argument_limit = NULL;
    const char *argument_path = NULL;
    gboolean unlimit = FALSE;
    gboolean global = FALSE;
    gboolean update_database = FALSE;
    FsearchDatabaseRefreshOverride refresh_override = {0};

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
        if (g_strcmp0(argv[i], "--refresh") == 0 || g_strcmp0(argv[i], "--refresh-index") == 0) {
            refresh_override.mode_is_set = TRUE;
            refresh_override.mode = FSEARCH_DATABASE_REFRESH_MODE_ALWAYS;
            continue;
        }
        if (g_strcmp0(argv[i], "--no-reindex") == 0) {
            refresh_override.mode_is_set = TRUE;
            refresh_override.mode = FSEARCH_DATABASE_REFRESH_MODE_NEVER;
            continue;
        }
        if (g_strcmp0(argv[i], "--reindex-interval") == 0) {
            if (i + 1 >= argc || !parse_refresh_interval(argv[i + 1], &refresh_override.interval_seconds)) {
                fsearch_cli_printerr("fsearch: --reindex-interval requires a non-negative number of seconds\n");
                return EXIT_FAILURE;
            }
            refresh_override.interval_is_set = TRUE;
            i++;
            continue;
        }
        if (g_strcmp0(argv[i], "--update-database") == 0 || g_strcmp0(argv[i], "-u") == 0) {
            update_database = TRUE;
            continue;
        }

        fsearch_cli_printerr("fsearch: unknown CLI option: %s\n", argv[i]);
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

    g_autofree char *database_dir = g_build_filename(g_get_user_data_dir(), "fsearch", NULL);
    g_autofree char *database_path = g_build_filename(database_dir, "fsearch.db", NULL);
    const FsearchDatabaseRefreshPolicy configured_refresh_policy = {
        .mode = run.config->database_refresh_mode,
        .interval_seconds = run.config->database_refresh_interval,
    };
    const FsearchDatabaseRefreshPolicy refresh_policy = fsearch_database_refresh_policy_resolve(configured_refresh_policy,
                                                                                                  g_getenv("FSEARCH_REFRESH_MODE"),
                                                                                                  g_getenv("FSEARCH_REFRESH_INTERVAL"),
                                                                                                  &refresh_override);
    const FsearchDatabaseRefreshIndexState index_state = fsearch_database_refresh_index_state_inspect(database_path,
                                                                                                         run.config->includes,
                                                                                                         run.config->excludes);
    run.refresh_action = update_database ? FSEARCH_DATABASE_REFRESH_ACTION_REFRESH
                                         : fsearch_database_refresh_policy_decide(refresh_policy.mode,
                                                                                  refresh_policy.interval_seconds,
                                                                                  &index_state,
                                                                                  g_get_real_time() / G_USEC_PER_SEC);
    if (run.refresh_action == FSEARCH_DATABASE_REFRESH_ACTION_FAIL) {
        fsearch_cli_printerr("fsearch: index is unavailable and automatic reindexing is disabled\n");
        config_free(run.config);
        g_main_loop_unref(run.loop);
        return EXIT_FAILURE;
    }
    run.wait_for_index_update = run.refresh_action == FSEARCH_DATABASE_REFRESH_ACTION_REFRESH;

    if (g_mkdir_with_parents(database_dir, 0700) != 0) {
        fsearch_cli_printerr("fsearch: failed to create data directory: %s\n", database_dir);
        config_free(run.config);
        g_main_loop_unref(run.loop);
        return EXIT_FAILURE;
    }
    g_autoptr(GFile) database_file = g_file_new_for_path(database_path);
    run.database = fsearch_database_new(g_steal_pointer(&database_file), run.config->includes, run.config->excludes);
    fsearch_database_set_load_policy(run.database,
                                     run.refresh_action == FSEARCH_DATABASE_REFRESH_ACTION_USE_STALE,
                                     FALSE);
    g_signal_connect(run.database, "load-finished", G_CALLBACK(on_database_loaded), &run);
    g_signal_connect(run.database, "scan-started", G_CALLBACK(on_database_scan_started), &run);
    g_signal_connect(run.database, "scan-finished", G_CALLBACK(on_database_scan_finished), &run);
    g_signal_connect(run.database, "save-finished", G_CALLBACK(on_database_save_finished), &run);
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
