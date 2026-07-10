#include "fsearch_cli.h"
#include "fsearch_cli_config.h"
#include "fsearch_config.h"
#include "fsearch_database_include.h"
#include "fsearch_database_include_manager.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#ifndef G_OS_WIN32
#include <signal.h>
#endif

static GString *captured_stdout;
static GString *captured_stderr;
static GMutex capture_mutex;

static void
capture_stdout(const gchar *text) {
    g_mutex_lock(&capture_mutex);
    g_string_append(captured_stdout, text);
    g_mutex_unlock(&capture_mutex);
}

static void
capture_stderr(const gchar *text) {
    g_mutex_lock(&capture_mutex);
    g_string_append(captured_stderr, text);
    g_mutex_unlock(&capture_mutex);
}

static void
test_frontend_selection(void) {
    const char *default_argv[] = {"fsearch"};
    g_assert_cmpint(fsearch_cli_select_frontend(G_N_ELEMENTS(default_argv), default_argv, NULL),
                    ==,
                    FSEARCH_CLI_FRONTEND_GUI);

    const char *env_cli_argv[] = {"fsearch"};
    g_assert_cmpint(fsearch_cli_select_frontend(G_N_ELEMENTS(env_cli_argv), env_cli_argv, "CLI"),
                    ==,
                    FSEARCH_CLI_FRONTEND_CLI);

    const char *gui_wins_argv[] = {"fsearch", "--cli", "--gui"};
    g_assert_cmpint(fsearch_cli_select_frontend(G_N_ELEMENTS(gui_wins_argv), gui_wins_argv, "CLI"),
                    ==,
                    FSEARCH_CLI_FRONTEND_GUI);

    const char *cli_wins_argv[] = {"fsearch", "--gui", "--cli"};
    g_assert_cmpint(fsearch_cli_select_frontend(G_N_ELEMENTS(cli_wins_argv), cli_wins_argv, "GUI"),
                    ==,
                    FSEARCH_CLI_FRONTEND_CLI);

    const char *config_argv[] = {"fsearch", "config", "--help"};
    g_assert_cmpint(fsearch_cli_select_frontend(G_N_ELEMENTS(config_argv), config_argv, NULL),
                    ==,
                    FSEARCH_CLI_FRONTEND_CLI);

    const char *config_search_value_argv[] = {"fsearch", "--search", "config"};
    g_assert_cmpint(fsearch_cli_select_frontend(G_N_ELEMENTS(config_search_value_argv), config_search_value_argv, NULL),
                    ==,
                    FSEARCH_CLI_FRONTEND_GUI);
}

static void
test_config_detects_running_gui(void) {
    if (!g_test_subprocess()) {
        g_test_trap_subprocess(NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
        g_test_trap_assert_passed();
        return;
    }

    g_autoptr(GTestDBus) test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(test_bus);

    g_autoptr(GError) error = NULL;
    g_autoptr(GDBusConnection) connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(connection);

    g_autoptr(GVariant) request_result = g_dbus_connection_call_sync(connection,
                                                                     "org.freedesktop.DBus",
                                                                     "/org/freedesktop/DBus",
                                                                     "org.freedesktop.DBus",
                                                                     "RequestName",
                                                                     g_variant_new("(su)", "io.github.cboxdoerfer.FSearch", 0),
                                                                     G_VARIANT_TYPE("(u)"),
                                                                     G_DBUS_CALL_FLAGS_NONE,
                                                                     -1,
                                                                     NULL,
                                                                     &error);
    g_assert_no_error(error);
    g_assert_nonnull(request_result);
    g_assert_true(fsearch_cli_config_gui_is_running());

    captured_stderr = g_string_new(NULL);
    GPrintFunc old_stderr_handler = g_set_printerr_handler(capture_stderr);
    char *config_argv[] = {"config", "set", "search.match-case", "true", NULL};
    g_assert_cmpint(fsearch_cli_config_run(4, config_argv), ==, 1);
    g_set_printerr_handler(old_stderr_handler);
    g_assert_cmpstr(captured_stderr->str,
                    ==,
                    "fsearch config: close the running FSearch GUI before changing shared settings\n");
    g_string_free(g_steal_pointer(&captured_stderr), TRUE);

    g_autoptr(GVariant) release_result = g_dbus_connection_call_sync(connection,
                                                                     "org.freedesktop.DBus",
                                                                     "/org/freedesktop/DBus",
                                                                     "org.freedesktop.DBus",
                                                                     "ReleaseName",
                                                                     g_variant_new("(s)", "io.github.cboxdoerfer.FSearch"),
                                                                     G_VARIANT_TYPE("(u)"),
                                                                     G_DBUS_CALL_FLAGS_NONE,
                                                                     -1,
                                                                     NULL,
                                                                     &error);
    g_assert_no_error(error);
    g_assert_nonnull(release_result);
    g_assert_false(fsearch_cli_config_gui_is_running());

    g_assert_true(g_dbus_connection_close_sync(connection, NULL, &error));
    g_assert_no_error(error);
    g_clear_object(&connection);
    g_test_dbus_down(test_bus);
}

static void
test_result_limit_precedence(void) {
    g_assert_cmpuint(fsearch_cli_resolve_result_limit(NULL, NULL, FALSE), ==, 100);
    g_assert_cmpuint(fsearch_cli_resolve_result_limit("25", NULL, FALSE), ==, 25);
    g_assert_cmpuint(fsearch_cli_resolve_result_limit("25", "7", FALSE), ==, 7);
    g_assert_cmpuint(fsearch_cli_resolve_result_limit("25", "0", FALSE), ==, 0);
    g_assert_cmpuint(fsearch_cli_resolve_result_limit("invalid", NULL, FALSE), ==, 100);
    g_assert_cmpuint(fsearch_cli_resolve_result_limit("25", "7", TRUE), ==, 0);
}

static void
test_cap_notice(void) {
    g_autofree char *notice = fsearch_cli_format_cap_notice(100);
    g_assert_cmpstr(notice,
                    ==,
                    "\x1b[3;37mResults capped at 100. Use --unlimit or --unlimited to get them all, or provide a custom --limit, "
                    "or set FSEARCH_RESULT_LIMIT.\x1b[0m");
}

static void
test_result_and_index_progress_notices(void) {
    g_autofree char *one_match = fsearch_cli_format_result_count_notice(1);
    g_assert_cmpstr(one_match, ==, "\x1b[3;37m1 match.\x1b[0m");

    g_autofree char *many_matches = fsearch_cli_format_result_count_notice(42);
    g_assert_cmpstr(many_matches, ==, "\x1b[3;37m42 matches.\x1b[0m");

    g_autofree char *progress = fsearch_cli_format_index_progress("18402 entries, 7150/s  /work/current/src");
    g_assert_cmpstr(progress, ==, "\r\x1b[2K\x1b[3;37mIndexing: 18402 entries, 7150/s  /work/current/src\x1b[0m");
}

static void
test_scoped_query(void) {
    g_autofree char *default_scope = fsearch_cli_build_scoped_query("invoice", "/work/current", FALSE);
    g_assert_cmpstr(default_scope, ==, "path:\"/work/current/\" AND (invoice)");

    g_autofree char *global_scope = fsearch_cli_build_scoped_query("invoice", "/work/current", TRUE);
    g_assert_cmpstr(global_scope, ==, "invoice");

    g_autofree char *escaped_scope = fsearch_cli_build_scoped_query("", "/work/\"quoted", FALSE);
    g_assert_cmpstr(escaped_scope, ==, "path:\"/work/\\\"quoted/\"");
}

#ifndef G_OS_WIN32
static gboolean
raise_sigint(gpointer user_data) {
    raise(SIGINT);
    return G_SOURCE_REMOVE;
}

static void
test_sigint_cancels_cli_run(void) {
    if (g_test_subprocess()) {
        g_idle_add(raise_sigint, NULL);
        char *argv[] = {"fsearch", "--cli", "--global", "--search", "needle", NULL};
        g_assert_cmpint(fsearch_cli_run(5, argv), ==, 130);
        return;
    }

    g_autofree char *tmp_dir = g_dir_make_tmp("fsearch-test-cli-sigint-XXXXXX", NULL);
    g_assert_nonnull(tmp_dir);
    g_autofree char *config_dir = g_build_filename(tmp_dir, "config", NULL);
    g_autofree char *data_dir = g_build_filename(tmp_dir, "data", NULL);
    g_autofree char *database_dir = g_build_filename(data_dir, "fsearch", NULL);
    g_assert_true(g_mkdir_with_parents(config_dir, 0700) == 0);
    g_assert_true(g_mkdir_with_parents(database_dir, 0700) == 0);

    g_autofree char *old_config_home = g_strdup(g_getenv("XDG_CONFIG_HOME"));
    g_autofree char *old_data_home = g_strdup(g_getenv("XDG_DATA_HOME"));
    g_setenv("XDG_CONFIG_HOME", config_dir, TRUE);
    g_setenv("XDG_DATA_HOME", data_dir, TRUE);
    g_test_trap_subprocess(NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
    if (old_config_home) {
        g_setenv("XDG_CONFIG_HOME", old_config_home, TRUE);
    }
    else {
        g_unsetenv("XDG_CONFIG_HOME");
    }
    if (old_data_home) {
        g_setenv("XDG_DATA_HOME", old_data_home, TRUE);
    }
    else {
        g_unsetenv("XDG_DATA_HOME");
    }
    g_test_trap_assert_passed();

    g_autofree char *database_path = g_build_filename(database_dir, "fsearch.db", NULL);
    g_unlink(database_path);
    g_rmdir(database_dir);
    g_rmdir(data_dir);
    g_rmdir(config_dir);
    g_rmdir(tmp_dir);
}
#endif

static void
test_search_rebuilds_missing_index_before_printing_results(void) {
    g_autofree char *tmp_dir = g_dir_make_tmp("fsearch-test-cli-XXXXXX", NULL);
    g_assert_nonnull(tmp_dir);
    g_autofree char *config_dir = g_build_filename(tmp_dir, "config", NULL);
    g_autofree char *data_dir = g_build_filename(tmp_dir, "data", NULL);
    g_autofree char *indexed_file = g_build_filename(tmp_dir, "needle.txt", NULL);
    g_assert_true(g_file_set_contents(indexed_file, "", 0, NULL));
    g_assert_true(g_mkdir_with_parents(config_dir, 0700) == 0);
    g_assert_true(g_mkdir_with_parents(data_dir, 0700) == 0);
    g_setenv("XDG_CONFIG_HOME", config_dir, TRUE);
    g_setenv("XDG_DATA_HOME", data_dir, TRUE);

    FsearchConfig *config = g_new0(FsearchConfig, 1);
    g_assert_true(config_load_default(config));
    g_clear_object(&config->includes);
    config->includes = fsearch_database_include_manager_new();
    g_autoptr(FsearchDatabaseInclude) include = fsearch_database_include_new(tmp_dir, TRUE, FALSE, FALSE, FALSE, 0);
    fsearch_database_include_manager_add(config->includes, include);
    g_assert_true(config_make_dir());
    g_assert_true(config_save(config));
    // Defaults retain this static string; production config loading replaces it with owned data.
    config->sort_by = NULL;
    config_free(config);

    g_autofree char *database_dir = g_build_filename(data_dir, "fsearch", NULL);
    g_assert_true(g_mkdir_with_parents(database_dir, 0700) == 0);

    captured_stdout = g_string_new(NULL);
    captured_stderr = g_string_new(NULL);
    GPrintFunc old_stdout_handler = g_set_print_handler(capture_stdout);
    GPrintFunc old_stderr_handler = g_set_printerr_handler(capture_stderr);

    char *argv[] = {"fsearch", "--cli", "--global", "--search", "needle", NULL};
    g_assert_cmpint(fsearch_cli_run(5, argv), ==, EXIT_SUCCESS);

    g_set_print_handler(old_stdout_handler);
    g_set_printerr_handler(old_stderr_handler);
    g_autofree char *expected_stdout = g_strdup_printf("%s\n", indexed_file);
    g_assert_nonnull(g_strstr_len(captured_stdout->str, -1, expected_stdout));
    g_assert_cmpstr(captured_stderr->str,
                    ==,
                    "\x1b[3;37mUpdating FSearch index before searching...\x1b[0m\n"
                    "\x1b[3;37m1 match.\x1b[0m\n");
    g_string_free(g_steal_pointer(&captured_stdout), TRUE);
    g_string_free(g_steal_pointer(&captured_stderr), TRUE);

    g_autofree char *database_path = g_build_filename(database_dir, "fsearch.db", NULL);
    g_unlink(database_path);
    g_rmdir(database_dir);
    g_unlink(indexed_file);
    g_rmdir(data_dir);
    g_autofree char *saved_config_dir = g_build_filename(config_dir, "fsearch", NULL);
    g_autofree char *saved_config_path = g_build_filename(saved_config_dir, "fsearch.conf", NULL);
    g_unlink(saved_config_path);
    g_rmdir(saved_config_dir);
    g_rmdir(config_dir);
    g_rmdir(tmp_dir);
}

static void
test_search_uses_shared_match_case_setting(void) {
    g_autofree char *tmp_dir = g_dir_make_tmp("fsearch-test-cli-shared-flags-XXXXXX", NULL);
    g_assert_nonnull(tmp_dir);
    g_autofree char *config_dir = g_build_filename(tmp_dir, "config", NULL);
    g_autofree char *data_dir = g_build_filename(tmp_dir, "data", NULL);
    g_autofree char *indexed_file = g_build_filename(tmp_dir, "Needle.txt", NULL);
    g_assert_true(g_file_set_contents(indexed_file, "", 0, NULL));
    g_assert_true(g_mkdir_with_parents(config_dir, 0700) == 0);
    g_assert_true(g_mkdir_with_parents(data_dir, 0700) == 0);
    g_setenv("XDG_CONFIG_HOME", config_dir, TRUE);
    g_setenv("XDG_DATA_HOME", data_dir, TRUE);

    FsearchConfig *config = g_new0(FsearchConfig, 1);
    g_assert_true(config_load_default(config));
    config->match_case = true;
    config->auto_match_case = false;
    g_clear_object(&config->includes);
    config->includes = fsearch_database_include_manager_new();
    g_autoptr(FsearchDatabaseInclude) include = fsearch_database_include_new(tmp_dir, TRUE, FALSE, FALSE, FALSE, 0);
    fsearch_database_include_manager_add(config->includes, include);
    g_assert_true(config_make_dir());
    g_assert_true(config_save(config));
    config->sort_by = NULL;
    config_free(config);

    g_autofree char *database_dir = g_build_filename(data_dir, "fsearch", NULL);
    g_assert_true(g_mkdir_with_parents(database_dir, 0700) == 0);

    captured_stdout = g_string_new(NULL);
    captured_stderr = g_string_new(NULL);
    GPrintFunc old_stdout_handler = g_set_print_handler(capture_stdout);
    GPrintFunc old_stderr_handler = g_set_printerr_handler(capture_stderr);

    char *argv[] = {"fsearch", "--cli", "--global", "--search", "needle", NULL};
    g_assert_cmpint(fsearch_cli_run(5, argv), ==, EXIT_SUCCESS);

    g_set_print_handler(old_stdout_handler);
    g_set_printerr_handler(old_stderr_handler);
    g_assert_null(g_strstr_len(captured_stdout->str, -1, indexed_file));
    g_assert_cmpstr(captured_stderr->str,
                    ==,
                    "\x1b[3;37mUpdating FSearch index before searching...\x1b[0m\n"
                    "\x1b[3;37m0 matches.\x1b[0m\n");
    g_string_free(g_steal_pointer(&captured_stdout), TRUE);
    g_string_free(g_steal_pointer(&captured_stderr), TRUE);

    g_autofree char *database_path = g_build_filename(database_dir, "fsearch.db", NULL);
    g_unlink(database_path);
    g_rmdir(database_dir);
    g_unlink(indexed_file);
    g_rmdir(data_dir);
    g_autofree char *saved_config_dir = g_build_filename(config_dir, "fsearch", NULL);
    g_autofree char *saved_config_path = g_build_filename(saved_config_dir, "fsearch.conf", NULL);
    g_unlink(saved_config_path);
    g_rmdir(saved_config_dir);
    g_rmdir(config_dir);
    g_rmdir(tmp_dir);
}

int
main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/FSearch/cli/frontend_selection", test_frontend_selection);
    g_test_add_func("/FSearch/cli/config_detects_running_gui", test_config_detects_running_gui);
    g_test_add_func("/FSearch/cli/result_limit_precedence", test_result_limit_precedence);
    g_test_add_func("/FSearch/cli/cap_notice", test_cap_notice);
    g_test_add_func("/FSearch/cli/result_and_index_progress_notices", test_result_and_index_progress_notices);
    g_test_add_func("/FSearch/cli/scoped_query", test_scoped_query);
#ifndef G_OS_WIN32
    g_test_add_func("/FSearch/cli/sigint_cancels_cli_run", test_sigint_cancels_cli_run);
#endif
    g_test_add_func("/FSearch/cli/search_rebuilds_missing_index_before_printing_results",
                    test_search_rebuilds_missing_index_before_printing_results);
    g_test_add_func("/FSearch/cli/search_uses_shared_match_case_setting", test_search_uses_shared_match_case_setting);

    return g_test_run();
}
