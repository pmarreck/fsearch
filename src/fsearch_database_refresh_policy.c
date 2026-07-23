#include "fsearch_database_refresh_policy.h"

#include "fsearch_database_file.h"
#include "fsearch_database_index_properties.h"

#include <errno.h>

static gboolean
parse_interval_seconds(const char *value, gint64 *interval_out) {
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

const char *
fsearch_database_refresh_mode_to_string(FsearchDatabaseRefreshMode mode) {
    switch (mode) {
    case FSEARCH_DATABASE_REFRESH_MODE_AUTO:
        return "auto";
    case FSEARCH_DATABASE_REFRESH_MODE_ALWAYS:
        return "always";
    case FSEARCH_DATABASE_REFRESH_MODE_NEVER:
        return "never";
    }
    return "auto";
}

gboolean
fsearch_database_refresh_mode_from_string(const char *value, FsearchDatabaseRefreshMode *mode_out) {
    g_return_val_if_fail(mode_out != NULL, FALSE);
    if (!value) {
        return FALSE;
    }

    if (g_ascii_strcasecmp(value, "auto") == 0) {
        *mode_out = FSEARCH_DATABASE_REFRESH_MODE_AUTO;
        return TRUE;
    }
    if (g_ascii_strcasecmp(value, "always") == 0) {
        *mode_out = FSEARCH_DATABASE_REFRESH_MODE_ALWAYS;
        return TRUE;
    }
    if (g_ascii_strcasecmp(value, "never") == 0) {
        *mode_out = FSEARCH_DATABASE_REFRESH_MODE_NEVER;
        return TRUE;
    }
    return FALSE;
}

/// Resolves saved, environmental, and one-shot policy inputs in precedence order without frontend state.
FsearchDatabaseRefreshPolicy
fsearch_database_refresh_policy_resolve(FsearchDatabaseRefreshPolicy configured,
                                        const char *environment_mode,
                                        const char *environment_interval,
                                        const FsearchDatabaseRefreshOverride *argument_override) {
    FsearchDatabaseRefreshPolicy resolved = configured;
    FsearchDatabaseRefreshMode environment_mode_value = FSEARCH_DATABASE_REFRESH_MODE_AUTO;
    gint64 environment_interval_value = 0;

    if (fsearch_database_refresh_mode_from_string(environment_mode, &environment_mode_value)) {
        resolved.mode = environment_mode_value;
    }
    if (parse_interval_seconds(environment_interval, &environment_interval_value)) {
        resolved.interval_seconds = environment_interval_value;
    }
    if (argument_override) {
        if (argument_override->mode_is_set) {
            resolved.mode = argument_override->mode;
        }
        if (argument_override->interval_is_set) {
            resolved.interval_seconds = argument_override->interval_seconds;
        }
    }
    return resolved;
}

/// Inspects serialized root metadata once so every frontend classifies index freshness identically.
FsearchDatabaseRefreshIndexState
fsearch_database_refresh_index_state_inspect(const char *database_path,
                                              FsearchDatabaseIncludeManager *configured_includes,
                                              FsearchDatabaseExcludeManager *configured_excludes) {
    FsearchDatabaseRefreshIndexState state = {0};
    g_return_val_if_fail(database_path != NULL, state);
    g_return_val_if_fail(configured_includes != NULL, state);
    g_return_val_if_fail(configured_excludes != NULL, state);

    g_autoptr(FsearchDatabaseIncludeManager) indexed_includes = NULL;
    g_autoptr(FsearchDatabaseExcludeManager) indexed_excludes = NULL;
    FsearchDatabaseIndexPropertyFlags flags = DATABASE_INDEX_PROPERTY_FLAG_NONE;
    if (!fsearch_database_file_load_config(database_path, &indexed_includes, &indexed_excludes, &flags)) {
        return state;
    }

    state.readable = TRUE;
    state.configuration_matches = fsearch_database_include_manager_equal(indexed_includes, configured_includes)
                               && fsearch_database_exclude_manager_equal(indexed_excludes, configured_excludes);
    if (!state.configuration_matches) {
        return state;
    }

    state.oldest_scan_time = G_MAXINT64;
    GPtrArray *includes = fsearch_database_include_manager_get_includes(indexed_includes);
    for (guint i = 0; includes && i < includes->len; i++) {
        FsearchDatabaseInclude *include = g_ptr_array_index(includes, i);
        if (fsearch_database_include_get_active(include)) {
            state.oldest_scan_time = MIN(state.oldest_scan_time, fsearch_database_include_get_last_scan_time(include));
        }
    }
    return state;
}

/// Decides whether a frontend must refresh, may use a stale index, or must fail without performing I/O.
FsearchDatabaseRefreshAction
fsearch_database_refresh_policy_decide(FsearchDatabaseRefreshMode mode,
                                       gint64 interval_seconds,
                                       const FsearchDatabaseRefreshIndexState *index_state,
                                       gint64 now_seconds) {
    g_return_val_if_fail(index_state != NULL, FSEARCH_DATABASE_REFRESH_ACTION_FAIL);

    if (mode == FSEARCH_DATABASE_REFRESH_MODE_ALWAYS) {
        return FSEARCH_DATABASE_REFRESH_ACTION_REFRESH;
    }
    if (mode == FSEARCH_DATABASE_REFRESH_MODE_NEVER) {
        if (!index_state->readable) {
            return FSEARCH_DATABASE_REFRESH_ACTION_FAIL;
        }
        if (!index_state->configuration_matches || (interval_seconds > 0 && index_state->oldest_scan_time <= now_seconds - interval_seconds)) {
            return FSEARCH_DATABASE_REFRESH_ACTION_USE_STALE;
        }
        return FSEARCH_DATABASE_REFRESH_ACTION_NONE;
    }

    if (!index_state->readable || !index_state->configuration_matches) {
        return FSEARCH_DATABASE_REFRESH_ACTION_REFRESH;
    }
    if (interval_seconds > 0 && index_state->oldest_scan_time <= now_seconds - interval_seconds) {
        return FSEARCH_DATABASE_REFRESH_ACTION_REFRESH;
    }
    return FSEARCH_DATABASE_REFRESH_ACTION_NONE;
}
