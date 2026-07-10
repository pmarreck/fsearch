#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    FSEARCH_CLI_FRONTEND_GUI,
    FSEARCH_CLI_FRONTEND_CLI,
} FsearchCliFrontend;

FsearchCliFrontend
fsearch_cli_select_frontend(guint argc, const char *const argv[], const char *ui_environment);

guint
fsearch_cli_resolve_result_limit(const char *environment_limit, const char *argument_limit, gboolean unlimit);

char *
fsearch_cli_format_cap_notice(guint limit);

char *
fsearch_cli_format_result_count_notice(guint total);

char *
fsearch_cli_format_index_update_notice(void);

char *
fsearch_cli_format_index_progress(const char *status);

char *
fsearch_cli_build_scoped_query(const char *query, const char *scope_path, gboolean global);

int
fsearch_cli_run(int argc, char *argv[]);

G_END_DECLS
