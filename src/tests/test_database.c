/*
 * Reproduces a bug where a "get item info" request for a row that no longer exists in the
 * current search view (e.g. because a newer or cancelled search replaced it while the request
 * was in flight) never got a response at all.
 *
 * fsearch_database.c's handle_work_in_worker_thread_cb() only emitted "item-info-ready" when
 * database_get_entry_info() actually found an entry; a NULL result (out-of-range index) was
 * silently dropped. On the UI side (fsearch_result_view.c), a NULL placeholder is cached the
 * moment a request is queued, specifically to avoid re-requesting the same row while it's
 * pending. Combined, a request that resolves to "not found" left that placeholder in place
 * forever -- the row rendered blank and never got retried until the entire row-info cache
 * happened to be wiped by some unrelated later search/sort completing.
 *
 * The fix makes the database always emit "item-info-ready" (with the row index and a NULL info
 * payload on failure), so callers can tell "no answer yet" apart from "definitively not found"
 * and clear their placeholder to allow a retry instead of getting stuck.
 */

#include "fsearch_database.h"
#include "fsearch_database_entry_info.h"
#include "fsearch_database_exclude_manager.h"
#include "fsearch_database_include.h"
#include "fsearch_database_include_manager.h"
#include "fsearch_database_work.h"
#include "fsearch_filter_manager.h"
#include "fsearch_query.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>

#define TEST_TIMEOUT_SECONDS 10

typedef struct {
    GMainLoop *loop;
    gboolean got_signal;
    gboolean timed_out;
    guint id;
    guint idx;
    FsearchDatabaseSearchInfo *search_info;
    FsearchDatabaseEntryInfo *item_info;
} WaitCtx;

static gboolean
on_timeout(gpointer user_data) {
    WaitCtx *ctx = user_data;
    ctx->timed_out = TRUE;
    g_main_loop_quit(ctx->loop);
    return G_SOURCE_REMOVE;
}

static void
on_search_finished(FsearchDatabase *db, guint id, FsearchDatabaseSearchInfo *info, gpointer user_data) {
    WaitCtx *ctx = user_data;
    ctx->got_signal = TRUE;
    ctx->id = id;
    ctx->search_info = info ? fsearch_database_search_info_ref(info) : NULL;
    g_main_loop_quit(ctx->loop);
}

static void
on_load_finished(FsearchDatabase *db, FsearchDatabaseInfo *info, gpointer user_data) {
    WaitCtx *ctx = user_data;
    ctx->got_signal = TRUE;
    g_main_loop_quit(ctx->loop);
}

static void
on_item_info_ready(FsearchDatabase *db, guint id, guint idx, FsearchDatabaseEntryInfo *info, gpointer user_data) {
    WaitCtx *ctx = user_data;
    ctx->got_signal = TRUE;
    ctx->id = id;
    ctx->idx = idx;
    ctx->item_info = info ? fsearch_database_entry_info_ref(info) : NULL;
    g_main_loop_quit(ctx->loop);
}

static void
wait_ctx_clear(WaitCtx *ctx) {
    g_clear_pointer(&ctx->search_info, fsearch_database_search_info_unref);
    g_clear_pointer(&ctx->item_info, fsearch_database_entry_info_unref);
    ctx->got_signal = FALSE;
}

// Pumps the default main context (where FsearchDatabase's async signals are delivered) until
// either the awaited signal fires or TEST_TIMEOUT_SECONDS elapses.
static void
wait_for_signal(WaitCtx *ctx) {
    ctx->timed_out = FALSE;
    ctx->loop = g_main_loop_new(NULL, FALSE);
    guint timeout_id = g_timeout_add_seconds(TEST_TIMEOUT_SECONDS, on_timeout, ctx);
    g_main_loop_run(ctx->loop);
    if (!ctx->timed_out) {
        g_source_remove(timeout_id);
    }
    g_clear_pointer(&ctx->loop, g_main_loop_unref);
    g_assert_false(ctx->timed_out);
}

static char *
test_file_path(const char *tmp_dir, int i) {
    g_autofree char *name = g_strdup_printf("file_%d.txt", i);
    return g_build_filename(tmp_dir, name, NULL);
}

static void
test_get_item_info_for_stale_index_still_signals(void) {
    g_autofree char *tmp_dir = g_dir_make_tmp("fsearch-test-database-XXXXXX", NULL);
    g_assert_nonnull(tmp_dir);
    for (int i = 0; i < 3; i++) {
        g_autofree char *path = test_file_path(tmp_dir, i);
        g_assert_true(g_file_set_contents(path, "", 0, NULL));
    }

    g_autoptr(FsearchDatabaseIncludeManager) include_manager = fsearch_database_include_manager_new();
    g_autoptr(FsearchDatabaseInclude) include = fsearch_database_include_new(tmp_dir, TRUE, FALSE, FALSE, FALSE, 0);
    fsearch_database_include_manager_add(include_manager, include);

    g_autoptr(FsearchDatabaseExcludeManager) exclude_manager = fsearch_database_exclude_manager_new();

    g_autofree char *db_path = g_build_filename(tmp_dir, "fsearch-test.db", NULL);
    g_autoptr(GFile) db_file = g_file_new_for_path(db_path);

    // fsearch_database_new() takes ownership of the GFile (matching every real caller, which
    // hands it off via g_steal_pointer() rather than lending it) -- g_steal_pointer() here keeps
    // that contract instead of leaving db_file's g_autoptr to double-free it later.
    //
    // Not a g_autoptr itself: disposing FsearchDatabase always performs a final save-to-file, so
    // that must happen (via the explicit g_object_unref below) before the temp directory is torn
    // down.
    FsearchDatabase *db = fsearch_database_new(g_steal_pointer(&db_file), include_manager, exclude_manager);
    g_assert_cmpint(fsearch_database_rescan_blocking(db), ==, FSEARCH_RESULT_SUCCESS);

    const guint view_id = 1;
    FsearchFilterManager *filters = fsearch_filter_manager_new_with_defaults();
    g_autoptr(FsearchQuery) query = fsearch_query_new("file_", NULL, filters, 0, "test");

    WaitCtx ctx = {};
    gulong search_finished_handler = g_signal_connect(db, "search-finished", G_CALLBACK(on_search_finished), &ctx);
    g_autoptr(FsearchDatabaseWork) search_work = fsearch_database_work_new_search(view_id,
                                                                                  query,
                                                                                  DATABASE_INDEX_PROPERTY_NAME,
                                                                                  GTK_SORT_ASCENDING);
    fsearch_database_queue_work(db, search_work);
    wait_for_signal(&ctx);
    g_signal_handler_disconnect(db, search_finished_handler);

    g_assert_true(ctx.got_signal);
    g_assert_nonnull(ctx.search_info);
    g_assert_cmpuint(fsearch_database_search_info_get_num_files(ctx.search_info), ==, 3);
    wait_ctx_clear(&ctx);

    gulong item_info_handler = g_signal_connect(db, "item-info-ready", G_CALLBACK(on_item_info_ready), &ctx);

    // Request info for an index that's clearly out of range for the 3-file result above. Before
    // the fix, database_get_entry_info() would return NULL and the signal would never be
    // emitted at all -- this wait would time out and ctx.got_signal would stay FALSE.
    const guint stale_idx = 999;
    g_autoptr(FsearchDatabaseWork) stale_item_info_work =
        fsearch_database_work_new_get_item_info(view_id, stale_idx, FSEARCH_DATABASE_ENTRY_INFO_FLAG_ALL);
    fsearch_database_queue_work(db, stale_item_info_work);
    wait_for_signal(&ctx);

    g_assert_true(ctx.got_signal);
    g_assert_cmpuint(ctx.id, ==, view_id);
    g_assert_cmpuint(ctx.idx, ==, stale_idx);
    g_assert_null(ctx.item_info);
    wait_ctx_clear(&ctx);

    // Sanity check: a valid index still resolves normally.
    g_autoptr(FsearchDatabaseWork) valid_item_info_work =
        fsearch_database_work_new_get_item_info(view_id, 0, FSEARCH_DATABASE_ENTRY_INFO_FLAG_ALL);
    fsearch_database_queue_work(db, valid_item_info_work);
    wait_for_signal(&ctx);

    g_assert_true(ctx.got_signal);
    g_assert_cmpuint(ctx.idx, ==, 0);
    g_assert_nonnull(ctx.item_info);
    wait_ctx_clear(&ctx);

    g_signal_handler_disconnect(db, item_info_handler);
    fsearch_filter_manager_unref(filters);

    // Disposing here (rather than via g_autoptr at function scope) performs the final
    // save-to-file while the temp directory still exists, before it's removed below.
    g_object_unref(db);

    for (int i = 0; i < 3; i++) {
        g_autofree char *path = test_file_path(tmp_dir, i);
        g_unlink(path);
    }
    g_unlink(db_path);
    g_rmdir(tmp_dir);
}

static void
test_rescan_with_unavailable_root_keeps_database_searchable(void) {
    g_autofree char *tmp_dir = g_dir_make_tmp("fsearch-test-database-XXXXXX", NULL);
    g_assert_nonnull(tmp_dir);
    g_autofree char *missing_root = g_build_filename(tmp_dir, "unavailable-root", NULL);
    g_autofree char *available_path = g_build_filename(tmp_dir, "available.txt", NULL);
    g_assert_true(g_file_set_contents(available_path, "", 0, NULL));

    g_autoptr(FsearchDatabaseIncludeManager) include_manager = fsearch_database_include_manager_new();
    g_autoptr(FsearchDatabaseInclude) available_include =
        fsearch_database_include_new(tmp_dir, TRUE, FALSE, FALSE, FALSE, 0);
    g_autoptr(FsearchDatabaseInclude) unavailable_include =
        fsearch_database_include_new(missing_root, TRUE, FALSE, FALSE, FALSE, 0);
    fsearch_database_include_manager_add(include_manager, available_include);
    fsearch_database_include_manager_add(include_manager, unavailable_include);
    g_autoptr(FsearchDatabaseExcludeManager) exclude_manager = fsearch_database_exclude_manager_new();

    g_autofree char *db_path = g_build_filename(tmp_dir, "fsearch-test.db", NULL);
    g_autoptr(GFile) db_file = g_file_new_for_path(db_path);
    FsearchDatabase *db = fsearch_database_new(g_steal_pointer(&db_file), include_manager, exclude_manager);

    g_test_expect_message("fsearch-database-scan", G_LOG_LEVEL_WARNING, "*doesn't exist");
    g_assert_cmpint(fsearch_database_rescan_blocking(db), ==, FSEARCH_RESULT_SUCCESS);
    g_test_assert_expected_messages();

    FsearchFilterManager *filters = fsearch_filter_manager_new_with_defaults();
    g_autoptr(FsearchQuery) query = fsearch_query_new("available", NULL, filters, 0, "test");
    WaitCtx ctx = {};
    gulong search_finished_handler = g_signal_connect(db, "search-finished", G_CALLBACK(on_search_finished), &ctx);
    g_autoptr(FsearchDatabaseWork) search_work = fsearch_database_work_new_search(1,
                                                                                  query,
                                                                                  DATABASE_INDEX_PROPERTY_NAME,
                                                                                  GTK_SORT_ASCENDING);
    fsearch_database_queue_work(db, search_work);
    wait_for_signal(&ctx);
    g_signal_handler_disconnect(db, search_finished_handler);

    g_assert_nonnull(ctx.search_info);
    g_assert_cmpuint(fsearch_database_search_info_get_num_files(ctx.search_info), ==, 1);
    wait_ctx_clear(&ctx);
    fsearch_filter_manager_unref(filters);

    g_object_unref(db);
    g_unlink(available_path);
    g_unlink(db_path);
    g_rmdir(tmp_dir);
}

static void
test_load_policy_can_use_stale_index_after_configuration_change(void) {
    g_autofree char *tmp_dir = g_dir_make_tmp("fsearch-test-database-stale-XXXXXX", NULL);
    g_assert_nonnull(tmp_dir);
    g_autofree char *indexed_file = g_build_filename(tmp_dir, "indexed.txt", NULL);
    g_assert_true(g_file_set_contents(indexed_file, "", 0, NULL));
    g_autofree char *db_path = g_build_filename(tmp_dir, "fsearch-test.db", NULL);

    g_autoptr(FsearchDatabaseIncludeManager) original_includes = fsearch_database_include_manager_new();
    g_autoptr(FsearchDatabaseInclude) original_include =
        fsearch_database_include_new(tmp_dir, TRUE, FALSE, FALSE, FALSE, 0);
    fsearch_database_include_manager_add(original_includes, original_include);
    g_autoptr(FsearchDatabaseExcludeManager) excludes = fsearch_database_exclude_manager_new();
    g_autoptr(GFile) original_file = g_file_new_for_path(db_path);
    FsearchDatabase *original = fsearch_database_new(g_steal_pointer(&original_file), original_includes, excludes);
    g_assert_cmpint(fsearch_database_rescan_blocking(original), ==, FSEARCH_RESULT_SUCCESS);
    g_object_unref(original);

    g_autoptr(FsearchDatabaseIncludeManager) changed_includes = fsearch_database_include_manager_new();
    g_autoptr(FsearchDatabaseInclude) changed_include =
        fsearch_database_include_new("/intentionally/different", TRUE, FALSE, FALSE, FALSE, 0);
    fsearch_database_include_manager_add(changed_includes, changed_include);
    g_autoptr(GFile) stale_file = g_file_new_for_path(db_path);
    FsearchDatabase *stale = fsearch_database_new(g_steal_pointer(&stale_file), changed_includes, excludes);
    fsearch_database_set_load_policy(stale, TRUE, FALSE);

    WaitCtx ctx = {};
    gulong load_finished_handler = g_signal_connect(stale, "load-finished", G_CALLBACK(on_load_finished), &ctx);
    g_autoptr(FsearchDatabaseWork) load_work = fsearch_database_work_new_load();
    fsearch_database_queue_work(stale, load_work);
    wait_for_signal(&ctx);
    g_signal_handler_disconnect(stale, load_finished_handler);

    g_autoptr(FsearchDatabaseInfo) info = NULL;
    g_assert_cmpint(fsearch_database_try_get_database_info(stale, &info), ==, FSEARCH_RESULT_SUCCESS);
    g_autoptr(FsearchDatabaseIncludeManager) loaded_includes = fsearch_database_info_get_include_manager(info);
    GPtrArray *loaded_roots = fsearch_database_include_manager_get_includes(loaded_includes);
    g_assert_cmpuint(loaded_roots->len, ==, 1);
    FsearchDatabaseInclude *loaded_root = g_ptr_array_index(loaded_roots, 0);
    g_assert_cmpstr(fsearch_database_include_get_path(loaded_root), ==, tmp_dir);

    g_object_unref(stale);
    g_unlink(db_path);
    g_unlink(indexed_file);
    g_rmdir(tmp_dir);
}

static void
test_dispose_persists_current_index(void) {
    g_autofree char *tmp_dir = g_dir_make_tmp("fsearch-test-database-dispose-XXXXXX", NULL);
    g_assert_nonnull(tmp_dir);
    g_autofree char *indexed_file = g_build_filename(tmp_dir, "indexed.txt", NULL);
    g_assert_true(g_file_set_contents(indexed_file, "", 0, NULL));
    g_autofree char *db_path = g_build_filename(tmp_dir, "fsearch-test.db", NULL);

    g_autoptr(FsearchDatabaseIncludeManager) includes = fsearch_database_include_manager_new();
    g_autoptr(FsearchDatabaseInclude) include = fsearch_database_include_new(tmp_dir, TRUE, FALSE, FALSE, FALSE, 0);
    fsearch_database_include_manager_add(includes, include);
    g_autoptr(FsearchDatabaseExcludeManager) excludes = fsearch_database_exclude_manager_new();
    g_autoptr(GFile) db_file = g_file_new_for_path(db_path);
    FsearchDatabase *db = fsearch_database_new(g_steal_pointer(&db_file), includes, excludes);
    g_assert_cmpint(fsearch_database_rescan_blocking(db), ==, FSEARCH_RESULT_SUCCESS);
    g_assert_cmpint(g_unlink(db_path), ==, 0);

    g_object_unref(db);
    g_assert_true(g_file_test(db_path, G_FILE_TEST_IS_REGULAR));

    g_unlink(db_path);
    g_unlink(indexed_file);
    g_rmdir(tmp_dir);
}

int
main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/FSearch/database/get_item_info_for_stale_index_still_signals",
                    test_get_item_info_for_stale_index_still_signals);
    g_test_add_func("/FSearch/database/rescan_with_unavailable_root_keeps_database_searchable",
                    test_rescan_with_unavailable_root_keeps_database_searchable);
    g_test_add_func("/FSearch/database/load_policy_can_use_stale_index_after_configuration_change",
                    test_load_policy_can_use_stale_index_after_configuration_change);
    g_test_add_func("/FSearch/database/dispose_persists_current_index", test_dispose_persists_current_index);

    return g_test_run();
}
