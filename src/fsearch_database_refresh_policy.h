#pragma once

#include <glib.h>

#include "fsearch_database_exclude_manager.h"
#include "fsearch_database_include_manager.h"

G_BEGIN_DECLS

#define FSEARCH_DATABASE_REFRESH_DEFAULT_INTERVAL_SECONDS (2 * 60 * 60)

typedef enum {
    FSEARCH_DATABASE_REFRESH_MODE_AUTO,
    FSEARCH_DATABASE_REFRESH_MODE_ALWAYS,
    FSEARCH_DATABASE_REFRESH_MODE_NEVER,
} FsearchDatabaseRefreshMode;

typedef enum {
    FSEARCH_DATABASE_REFRESH_ACTION_NONE,
    FSEARCH_DATABASE_REFRESH_ACTION_REFRESH,
    FSEARCH_DATABASE_REFRESH_ACTION_USE_STALE,
    FSEARCH_DATABASE_REFRESH_ACTION_FAIL,
} FsearchDatabaseRefreshAction;

typedef struct {
    gboolean readable;
    gboolean configuration_matches;
    gint64 oldest_scan_time;
} FsearchDatabaseRefreshIndexState;

typedef struct {
    FsearchDatabaseRefreshMode mode;
    gint64 interval_seconds;
} FsearchDatabaseRefreshPolicy;

typedef struct {
    gboolean mode_is_set;
    FsearchDatabaseRefreshMode mode;
    gboolean interval_is_set;
    gint64 interval_seconds;
} FsearchDatabaseRefreshOverride;

const char *
fsearch_database_refresh_mode_to_string(FsearchDatabaseRefreshMode mode);

gboolean
fsearch_database_refresh_mode_from_string(const char *value, FsearchDatabaseRefreshMode *mode_out);

FsearchDatabaseRefreshPolicy
fsearch_database_refresh_policy_resolve(FsearchDatabaseRefreshPolicy configured,
                                        const char *environment_mode,
                                        const char *environment_interval,
                                        const FsearchDatabaseRefreshOverride *argument_override);

FsearchDatabaseRefreshIndexState
fsearch_database_refresh_index_state_inspect(const char *database_path,
                                              FsearchDatabaseIncludeManager *configured_includes,
                                              FsearchDatabaseExcludeManager *configured_excludes);

FsearchDatabaseRefreshAction
fsearch_database_refresh_policy_decide(FsearchDatabaseRefreshMode mode,
                                       gint64 interval_seconds,
                                       const FsearchDatabaseRefreshIndexState *index_state,
                                       gint64 now_seconds);

G_END_DECLS
