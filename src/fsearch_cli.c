#include "fsearch_cli.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define FSEARCH_CLI_DEFAULT_RESULT_LIMIT 100

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
