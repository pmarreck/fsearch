#include "fsearch_cli.h"

#include <glib.h>

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
                    "\x1b[3;90mResults capped at 100. Use --unlimit to get them all, or provide a custom --limit, "
                    "or set FSEARCH_RESULT_LIMIT.\x1b[0m");
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

int
main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/FSearch/cli/frontend_selection", test_frontend_selection);
    g_test_add_func("/FSearch/cli/result_limit_precedence", test_result_limit_precedence);
    g_test_add_func("/FSearch/cli/cap_notice", test_cap_notice);
    g_test_add_func("/FSearch/cli/scoped_query", test_scoped_query);

    return g_test_run();
}
