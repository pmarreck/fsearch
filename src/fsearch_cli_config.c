/*
   FSearch - A fast file search utility
   Copyright © 2026 Christian Boxdörfer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
   */

#include "fsearch_cli_config.h"
#include "fsearch_cli.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fsearch_config.h"
#include "fsearch_database_exclude.h"
#include "fsearch_database_exclude_manager.h"
#include "fsearch_database_file.h"
#include "fsearch_database_include.h"
#include "fsearch_database_include_manager.h"
#include "fsearch_database_index_properties.h"
#include "fsearch_filter.h"
#include "fsearch_filter_manager.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { SCALAR_BOOL, SCALAR_INT, SCALAR_UINT, SCALAR_STRING, SCALAR_OPEN_ACTION } ScalarType;

typedef struct {
    const char *key;
    ScalarType type;
    size_t offset;
} ScalarSetting;

#define BOOL_SETTING(key, member) {key, SCALAR_BOOL, offsetof(FsearchConfig, member)}
#define INT_SETTING(key, member) {key, SCALAR_INT, offsetof(FsearchConfig, member)}
#define UINT_SETTING(key, member) {key, SCALAR_UINT, offsetof(FsearchConfig, member)}
#define STRING_SETTING(key, member) {key, SCALAR_STRING, offsetof(FsearchConfig, member)}

static const ScalarSetting scalar_settings[] = {
    BOOL_SETTING("search.hide-empty-results", hide_results_on_empty_search),
    BOOL_SETTING("search.in-path", search_in_path),
    BOOL_SETTING("search.regex", enable_regex),
    BOOL_SETTING("search.match-case", match_case),
    BOOL_SETTING("search.auto-path", auto_search_in_path),
    BOOL_SETTING("search.auto-case", auto_match_case),
    BOOL_SETTING("search.as-you-type", search_as_you_type),
    STRING_SETTING("application.folder-open-command", folder_open_cmd),
    BOOL_SETTING("window.restore-size", restore_window_size),
    INT_SETTING("window.width", window_width),
    INT_SETTING("window.height", window_height),
    BOOL_SETTING("ui.binary-units", show_base_2_units),
    BOOL_SETTING("ui.highlight-search-terms", highlight_search_terms),
    BOOL_SETTING("ui.single-click-open", single_click_open),
    BOOL_SETTING("ui.launch-desktop-files", launch_desktop_files),
    BOOL_SETTING("ui.dark-theme", enable_dark_theme),
    BOOL_SETTING("ui.list-tooltips", enable_list_tooltips),
    BOOL_SETTING("ui.restore-columns", restore_column_config),
    BOOL_SETTING("ui.restore-sort", restore_sort_order),
    BOOL_SETTING("ui.double-click-path", double_click_path),
    {"ui.after-open", SCALAR_OPEN_ACTION, offsetof(FsearchConfig, action_after_file_open)},
    BOOL_SETTING("ui.after-open-keyboard", action_after_file_open_keyboard),
    BOOL_SETTING("ui.after-open-mouse", action_after_file_open_mouse),
    BOOL_SETTING("ui.exit-on-escape", exit_on_escape),
    BOOL_SETTING("ui.indexing-status", show_indexing_status),
    BOOL_SETTING("ui.menubar", show_menubar),
    BOOL_SETTING("ui.statusbar", show_statusbar),
    BOOL_SETTING("ui.filter", show_filter),
    BOOL_SETTING("ui.search-button", show_search_button),
    BOOL_SETTING("ui.list-icons", show_listview_icons),
    BOOL_SETTING("columns.path.visible", show_path_column),
    BOOL_SETTING("columns.type.visible", show_type_column),
    BOOL_SETTING("columns.extension.visible", show_extension_column),
    BOOL_SETTING("columns.size.visible", show_size_column),
    BOOL_SETTING("columns.modified.visible", show_modified_column),
    STRING_SETTING("sort.by", sort_by),
    BOOL_SETTING("sort.ascending", sort_ascending),
    UINT_SETTING("columns.name.width", name_column_width),
    UINT_SETTING("columns.path.width", path_column_width),
    UINT_SETTING("columns.type.width", type_column_width),
    UINT_SETTING("columns.extension.width", extension_column_width),
    UINT_SETTING("columns.size.width", size_column_width),
    UINT_SETTING("columns.modified.width", modified_column_width),
    UINT_SETTING("columns.name.position", name_column_pos),
    UINT_SETTING("columns.path.position", path_column_pos),
    UINT_SETTING("columns.type.position", type_column_pos),
    UINT_SETTING("columns.size.position", size_column_pos),
    UINT_SETTING("columns.modified.position", modified_column_pos),
    BOOL_SETTING("dialogs.failed-open", show_dialog_failed_opening),
};

static void
print_help(void) {
    g_print("%s", _("Usage: fsearch config COMMAND [OPTIONS]\n\n"
            "View and modify the shared FSearch GUI and CLI configuration.\n\n"
            "Configuration commands:\n"
            "  path                         Print the configuration file path\n"
            "  show [SECTION] [--json]      Show settings\n"
            "  get KEY [--json]             Print one scalar setting\n"
            "  set KEY VALUE                Set one scalar setting\n"
            "  unset KEY                    Restore one scalar setting default\n"
            "  reset [KEY|SECTION] --yes [--rescan]\n"
            "                               Restore defaults\n"
            "  doctor                       Check configuration and index consistency\n"
            "  roots list|add|set|remove    Manage indexed roots\n"
            "  excludes list|add|remove|hidden\n"
            "                               Manage index exclusions\n"
            "  filters list|add|set|remove|reset\n"
            "                               Manage saved search filters\n\n"
            "Scalar values use true/false, decimal integers, or strings.\n"
            "Run 'fsearch config show' to list every scalar key and current value.\n"
            "Scalar sections: search, application, window, ui, columns, sort, dialogs.\n"
            "Use 'fsearch config roots|excludes|filters --help' for family options.\n"
            "Close the FSearch GUI before changing shared settings.\n"
            "Index changes become searchable on the next CLI search, or immediately with --rescan.\n"
            "Use --json with show, get, and collection list commands.\n\n"
            "Examples:\n"
            "  fsearch config show search --json\n"
            "  fsearch config get search.match-case\n"
            "  fsearch config set search.match-case true\n"
            "  fsearch config roots add ~/Code --monitor --one-file-system --rescan\n"
            "  fsearch config excludes add '*.tmp' --type wildcard --scope basename --target files\n"
            "  fsearch config filters add Documents --query 'ext:pdf' --macro docs\n"
            "  fsearch config reset search --yes\n"));
}

static gboolean
command_help_requested(int argc, char *argv[]) {
    return argc > 2 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0 || strcmp(argv[2], "help") == 0);
}

static void
print_scalar_command_help(const char *command) {
    if (strcmp(command, "show") == 0)
        g_print("%s", _("Usage: fsearch config show [SECTION] [--json]\nShow all scalar settings or one dotted section.\n"));
    else if (strcmp(command, "get") == 0)
        g_print("%s", _("Usage: fsearch config get KEY [--json]\nPrint one typed scalar value. Discover keys with 'fsearch "
                        "config show'.\n"));
    else if (strcmp(command, "set") == 0)
        g_print("%s", _("Usage: fsearch config set KEY VALUE\nSet a boolean, integer, string, or enumerated scalar value.\n"));
    else if (strcmp(command, "unset") == 0)
        g_print("%s", _("Usage: fsearch config unset KEY\nRestore one scalar setting to its built-in default.\n"));
    else if (strcmp(command, "reset") == 0)
        g_print("%s", _("Usage: fsearch config reset [KEY|SECTION] --yes [--rescan]\nReset is destructive and requires --yes. "
                        "--rescan applies to roots, database, excludes, or all.\n"));
    else if (strcmp(command, "doctor") == 0)
        g_print("%s", _("Usage: fsearch config doctor\nCheck configuration readability and whether the saved index uses the "
                        "same roots and exclusions.\n"));
}

static void
print_roots_help(void) {
    g_print("%s", _("Usage: fsearch config roots COMMAND [PATH] [OPTIONS]\n\n"
            "Commands:\n"
            "  list [--json]                List indexed roots\n"
            "  add PATH [OPTIONS]           Add a root directory\n"
            "  set PATH [OPTIONS]           Change an existing root\n"
            "  remove PATH [--rescan]       Remove an existing root\n\n"
            "Options for add and set:\n"
            "  --active, --inactive\n"
            "  --monitor, --no-monitor\n"
            "  --one-file-system, --no-one-file-system\n"
            "  --scan-after-launch, --no-scan-after-launch\n"
            "  --rescan-every SECONDS        Set periodic rescan interval; 0 disables it\n"
            "  --rescan                      Rebuild the index immediately after saving\n"));
}

static void
print_excludes_help(void) {
    g_print("%s", _("Usage: fsearch config excludes COMMAND [PATTERN|VALUE] [OPTIONS]\n\n"
            "Commands:\n"
            "  list [--json]                 List exclusions\n"
            "  add PATTERN [OPTIONS]         Add an exclusion\n"
            "  remove PATTERN [OPTIONS]      Remove a matching exclusion\n"
            "  hidden [true|false] [--rescan]\n"
            "                                View or change hidden-file exclusion\n\n"
            "Options for add and remove:\n"
            "  --type fixed|wildcard|regex   Default: fixed\n"
            "  --scope full-path|basename    Default: full-path\n"
            "  --target both|files|folders   Default: both\n"
            "  --active, --inactive          Add state or removal selector\n"
            "  --rescan                      Rebuild the index immediately after saving\n"));
}

static void
print_filters_help(void) {
    g_print("%s", _("Usage: fsearch config filters COMMAND [NAME] [OPTIONS]\n\n"
            "Commands:\n"
            "  list [--json]                 List saved filters\n"
            "  add NAME [OPTIONS]            Add a named filter\n"
            "  set NAME [OPTIONS]            Change a named filter\n"
            "  remove NAME                   Remove a named filter\n"
            "  reset                         Restore built-in filters\n\n"
            "Options for add and set:\n"
            "  --query QUERY                 Saved FSearch query\n"
            "  --macro MACRO                 Query macro name\n"
            "  --case, --no-case             Match case\n"
            "  --path, --no-path             Search full paths\n"
            "  --regex, --no-regex           Treat query as a regular expression\n"));
}

static char *
config_path(void) {
    return g_build_filename(g_get_user_config_dir(), "fsearch", "fsearch.conf", NULL);
}

static FsearchConfig *
load_config(gboolean *existed) {
    FsearchConfig *config = g_new0(FsearchConfig, 1);
    *existed = config_load(config);
    if (!*existed) {
        config_load_default(config);
    }
    return config;
}

gboolean
fsearch_cli_config_gui_is_running(void) {
    g_autoptr(GError) error = NULL;
    g_autoptr(GDBusConnection) connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!connection) {
        return FALSE;
    }

    g_autoptr(GVariant) result = g_dbus_connection_call_sync(connection,
                                                             "org.freedesktop.DBus",
                                                             "/org/freedesktop/DBus",
                                                             "org.freedesktop.DBus",
                                                             "NameHasOwner",
                                                             g_variant_new("(s)", APP_ID),
                                                             G_VARIANT_TYPE("(b)"),
                                                             G_DBUS_CALL_FLAGS_NONE,
                                                             -1,
                                                             NULL,
                                                             &error);
    if (!result) {
        return FALSE;
    }

    gboolean has_owner = FALSE;
    g_variant_get(result, "(b)", &has_owner);
    return has_owner;
}

static gboolean
save_config(FsearchConfig *config) {
    if (fsearch_cli_config_gui_is_running()) {
        fsearch_cli_printerr("fsearch config: close the running FSearch GUI before changing shared settings\n");
        return FALSE;
    }
    if (!config_make_dir() || !config_save(config)) {
        fsearch_cli_printerr("fsearch config: failed to save configuration\n");
        return FALSE;
    }
    return TRUE;
}

static int
finish_index_mutation(gboolean rescan) {
    if (rescan) {
        return FSEARCH_CLI_CONFIG_RESCAN_REQUESTED;
    }
    fsearch_cli_printerr("Index configuration changed; the next CLI search will rebuild it.\n");
    return 0;
}

static const ScalarSetting *
find_setting(const char *key) {
    for (guint i = 0; i < G_N_ELEMENTS(scalar_settings); i++) {
        if (g_strcmp0(key, scalar_settings[i].key) == 0) {
            return &scalar_settings[i];
        }
    }
    return NULL;
}

static const char *
action_name(FsearchConfigActionAfterOpen action) {
    return action == ACTION_AFTER_OPEN_MINIMIZE ? "minimize" : action == ACTION_AFTER_OPEN_CLOSE ? "close" : "nothing";
}

static char *
scalar_value(const ScalarSetting *setting, const FsearchConfig *config) {
    const void *ptr = (const char *)config + setting->offset;
    switch (setting->type) {
    case SCALAR_BOOL:
        return g_strdup(*(const bool *)ptr ? "true" : "false");
    case SCALAR_INT:
        return g_strdup_printf("%d", *(const int32_t *)ptr);
    case SCALAR_UINT:
        return g_strdup_printf("%u", *(const uint32_t *)ptr);
    case SCALAR_STRING:
        return g_strdup(*(char *const *)ptr ? *(char *const *)ptr : "");
    case SCALAR_OPEN_ACTION:
        return g_strdup(action_name(*(const FsearchConfigActionAfterOpen *)ptr));
    }
    return NULL;
}

static gboolean
parse_bool(const char *value, bool *out) {
    if (g_ascii_strcasecmp(value, "true") == 0 || g_ascii_strcasecmp(value, "on") == 0 || strcmp(value, "1") == 0) {
        *out = true;
        return TRUE;
    }
    if (g_ascii_strcasecmp(value, "false") == 0 || g_ascii_strcasecmp(value, "off") == 0 || strcmp(value, "0") == 0) {
        *out = false;
        return TRUE;
    }
    return FALSE;
}

static gboolean
set_scalar_value(const ScalarSetting *setting, FsearchConfig *config, const char *value) {
    void *ptr = (char *)config + setting->offset;
    char *end = NULL;
    switch (setting->type) {
    case SCALAR_BOOL:
        return parse_bool(value, ptr);
    case SCALAR_INT: {
        const gint64 parsed = g_ascii_strtoll(value, &end, 10);
        if (!end || *end || parsed < G_MININT32 || parsed > G_MAXINT32)
            return FALSE;
        *(int32_t *)ptr = (int32_t)parsed;
        return TRUE;
    }
    case SCALAR_UINT: {
        const guint64 parsed = g_ascii_strtoull(value, &end, 10);
        if (!end || *end || parsed > G_MAXUINT32)
            return FALSE;
        *(uint32_t *)ptr = (uint32_t)parsed;
        return TRUE;
    }
    case SCALAR_STRING:
        g_free(*(char **)ptr);
        *(char **)ptr = g_strdup(value);
        return TRUE;
    case SCALAR_OPEN_ACTION:
        if (strcmp(value, "nothing") == 0)
            *(FsearchConfigActionAfterOpen *)ptr = ACTION_AFTER_OPEN_NOTHING;
        else if (strcmp(value, "minimize") == 0)
            *(FsearchConfigActionAfterOpen *)ptr = ACTION_AFTER_OPEN_MINIMIZE;
        else if (strcmp(value, "close") == 0)
            *(FsearchConfigActionAfterOpen *)ptr = ACTION_AFTER_OPEN_CLOSE;
        else
            return FALSE;
        return TRUE;
    }
    return FALSE;
}

static void
copy_scalar_value(const ScalarSetting *setting, FsearchConfig *dest, const FsearchConfig *source) {
    void *dest_ptr = (char *)dest + setting->offset;
    const void *source_ptr = (const char *)source + setting->offset;
    switch (setting->type) {
    case SCALAR_BOOL:
        *(bool *)dest_ptr = *(const bool *)source_ptr;
        break;
    case SCALAR_INT:
        *(int32_t *)dest_ptr = *(const int32_t *)source_ptr;
        break;
    case SCALAR_UINT:
        *(uint32_t *)dest_ptr = *(const uint32_t *)source_ptr;
        break;
    case SCALAR_STRING:
        g_free(*(char **)dest_ptr);
        *(char **)dest_ptr = g_strdup(*(char *const *)source_ptr);
        break;
    case SCALAR_OPEN_ACTION:
        *(FsearchConfigActionAfterOpen *)dest_ptr = *(const FsearchConfigActionAfterOpen *)source_ptr;
        break;
    }
}

static char *
json_quote(const char *value) {
    GString *out = g_string_new("\"");
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p == '"' || *p == '\\')
            g_string_append_c(out, '\\');
        if (*p == '\n')
            g_string_append(out, "\\n");
        else if (*p == '\r')
            g_string_append(out, "\\r");
        else if (*p == '\t')
            g_string_append(out, "\\t");
        else if (*p < 0x20)
            g_string_append_printf(out, "\\u%04x", *p);
        else
            g_string_append_c(out, (char)*p);
    }
    g_string_append_c(out, '"');
    return g_string_free(out, FALSE);
}

static int
unknown_option(const char *command, const char *option) {
    fsearch_cli_printerr("fsearch config %s: unknown option: %s\n", command, option);
    return 2;
}

static int
unexpected_argument(const char *command, const char *argument) {
    fsearch_cli_printerr("fsearch config %s: unexpected argument: %s\n", command, argument);
    return 2;
}

static int
run_get(FsearchConfig *config, int argc, char *argv[]) {
    const char *key = NULL;
    gboolean json = FALSE;
    gboolean positional_only = FALSE;
    for (int i = 2; i < argc; i++) {
        if (!positional_only && strcmp(argv[i], "--") == 0) {
            positional_only = TRUE;
        }
        else if (!positional_only && strcmp(argv[i], "--json") == 0) {
            json = TRUE;
        }
        else if (!positional_only && argv[i][0] == '-') {
            return unknown_option("get", argv[i]);
        }
        else if (!key) {
            key = argv[i];
        }
        else {
            return unexpected_argument("get", argv[i]);
        }
    }
    if (!key) {
        fsearch_cli_printerr("Usage: fsearch config get KEY [--json]\n");
        return 2;
    }
    const ScalarSetting *setting = find_setting(key);
    if (!setting) {
        fsearch_cli_printerr("fsearch config: unknown setting: %s\n", key);
        return 2;
    }
    g_autofree char *value = scalar_value(setting, config);
    if (json) {
        g_autofree char *quoted = setting->type == SCALAR_BOOL || setting->type == SCALAR_INT
                                       || setting->type == SCALAR_UINT
                                    ? g_strdup(value)
                                    : json_quote(value);
        g_print("{\"%s\":%s}\n", setting->key, quoted);
    }
    else
        g_print("%s\n", value);
    return 0;
}

static int
run_show(FsearchConfig *config, int argc, char *argv[]) {
    gboolean json = FALSE;
    gboolean positional_only = FALSE;
    const char *section = NULL;
    for (int i = 2; i < argc; i++) {
        if (!positional_only && strcmp(argv[i], "--") == 0) {
            positional_only = TRUE;
        }
        else if (!positional_only && strcmp(argv[i], "--json") == 0) {
            json = TRUE;
        }
        else if (!positional_only && argv[i][0] == '-') {
            return unknown_option("show", argv[i]);
        }
        else if (!section) {
            section = argv[i];
        }
        else {
            return unexpected_argument("show", argv[i]);
        }
    }
    g_autofree char *section_prefix = section ? g_strdup_printf("%s.", section) : NULL;
    if (section) {
        gboolean found = FALSE;
        for (guint i = 0; i < G_N_ELEMENTS(scalar_settings); i++) {
            if (g_str_has_prefix(scalar_settings[i].key, section_prefix)) {
                found = TRUE;
                break;
            }
        }
        if (!found) {
            fsearch_cli_printerr("fsearch config show: unknown section: %s\n", section);
            return 2;
        }
    }
    if (json)
        g_print("{");
    gboolean first = TRUE;
    for (guint i = 0; i < G_N_ELEMENTS(scalar_settings); i++) {
        const ScalarSetting *setting = &scalar_settings[i];
        if (section && !g_str_has_prefix(setting->key, section_prefix))
            continue;
        g_autofree char *value = scalar_value(setting, config);
        if (json) {
            g_autofree char *encoded = setting->type == SCALAR_BOOL || setting->type == SCALAR_INT
                                            || setting->type == SCALAR_UINT
                                         ? g_strdup(value)
                                         : json_quote(value);
            g_print("%s\"%s\":%s", first ? "" : ",", setting->key, encoded);
        }
        else
            g_print("%s = %s\n", setting->key, value);
        first = FALSE;
    }
    if (json)
        g_print("}\n");
    return 0;
}

static int
run_set(FsearchConfig *config, int argc, char *argv[], gboolean reset) {
    const char *positionals[2] = {NULL, NULL};
    guint positional_count = 0;
    gboolean positional_only = FALSE;
    for (int i = 2; i < argc; i++) {
        if (!positional_only && strcmp(argv[i], "--") == 0) {
            positional_only = TRUE;
        }
        else if (!positional_only && argv[i][0] == '-') {
            return unknown_option(reset ? "unset" : "set", argv[i]);
        }
        else if (positional_count < G_N_ELEMENTS(positionals)) {
            positionals[positional_count++] = argv[i];
        }
        else {
            return unexpected_argument(reset ? "unset" : "set", argv[i]);
        }
    }
    if (positional_count != (reset ? 1 : 2)) {
        fsearch_cli_printerr("Usage: fsearch config %s KEY%s\n", reset ? "unset" : "set", reset ? "" : " VALUE");
        return 2;
    }
    const ScalarSetting *setting = find_setting(positionals[0]);
    if (!setting) {
        fsearch_cli_printerr("fsearch config: unknown setting: %s\n", positionals[0]);
        return 2;
    }
    g_autoptr(FsearchConfig) defaults = NULL;
    const char *value = positionals[1];
    if (reset) {
        defaults = g_new0(FsearchConfig, 1);
        config_load_default(defaults);
        copy_scalar_value(setting, config, defaults);
    }
    if (!reset && !set_scalar_value(setting, config, value)) {
        fsearch_cli_printerr("fsearch config: invalid value for %s: %s\n", setting->key, value);
        return 2;
    }
    if (!save_config(config))
        return 1;
    g_autofree char *saved = scalar_value(setting, config);
    fsearch_cli_printerr("%s = %s\n", setting->key, saved);
    return 0;
}

static FsearchDatabaseInclude *
find_root(FsearchConfig *config, const char *path) {
    g_autoptr(GPtrArray) roots = fsearch_database_include_manager_get_includes(config->includes);
    for (guint i = 0; i < roots->len; i++) {
        FsearchDatabaseInclude *root = g_ptr_array_index(roots, i);
        if (g_strcmp0(path, fsearch_database_include_get_path(root)) == 0)
            return fsearch_database_include_ref(root);
    }
    return NULL;
}

static int
run_roots(FsearchConfig *config, int argc, char *argv[]) {
    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_roots_help();
        return argc < 3 ? 2 : 0;
    }
    if (strcmp(argv[2], "list") == 0) {
        gboolean json = FALSE;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--json") == 0)
                json = TRUE;
            else if (argv[i][0] == '-')
                return unknown_option("roots list", argv[i]);
            else
                return unexpected_argument("roots list", argv[i]);
        }
        g_autoptr(GPtrArray) roots = fsearch_database_include_manager_get_includes(config->includes);
        if (json)
            g_print("[");
        for (guint i = 0; i < roots->len; i++) {
            FsearchDatabaseInclude *root = g_ptr_array_index(roots, i);
            g_autofree char *path = json_quote(fsearch_database_include_get_path(root));
            if (json)
                g_print("%s{\"path\":%s,\"active\":%s,\"monitor\":%s,\"one_file_system\":%s,\"scan_after_launch\":%s,"
                        "\"rescan_after\":%" G_GINT64_FORMAT "}",
                        i ? "," : "",
                        path,
                        fsearch_database_include_get_active(root) ? "true" : "false",
                        fsearch_database_include_get_monitored(root) ? "true" : "false",
                        fsearch_database_include_get_one_file_system(root) ? "true" : "false",
                        fsearch_database_include_get_scan_after_launch(root) ? "true" : "false",
                        fsearch_database_include_get_rescan_after(root));
            else
                g_print("%s\tactive=%s monitor=%s one-file-system=%s scan-after-launch=%s "
                        "rescan-after=%" G_GINT64_FORMAT "\n",
                        fsearch_database_include_get_path(root),
                        fsearch_database_include_get_active(root) ? "true" : "false",
                        fsearch_database_include_get_monitored(root) ? "true" : "false",
                        fsearch_database_include_get_one_file_system(root) ? "true" : "false",
                        fsearch_database_include_get_scan_after_launch(root) ? "true" : "false",
                        fsearch_database_include_get_rescan_after(root));
        }
        if (json)
            g_print("]\n");
        return 0;
    }
    const gboolean add = strcmp(argv[2], "add") == 0;
    const gboolean set = strcmp(argv[2], "set") == 0;
    const gboolean remove = strcmp(argv[2], "remove") == 0;
    if (!add && !set && !remove) {
        fsearch_cli_printerr("fsearch config roots: unknown operation: %s\n", argv[2]);
        return 2;
    }

    const char *path_arg = NULL;
    const char *rescan_value = NULL;
    gboolean positional_only = FALSE;
    gboolean active_set = FALSE;
    gboolean active_value = FALSE;
    gboolean monitor_set = FALSE;
    gboolean monitor_value = FALSE;
    gboolean onefs_set = FALSE;
    gboolean onefs_value = FALSE;
    gboolean scan_set = FALSE;
    gboolean scan_value = FALSE;
    gboolean rescan_now = FALSE;
    for (int i = 3; i < argc; i++) {
        if (!positional_only && strcmp(argv[i], "--") == 0) {
            positional_only = TRUE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--active") == 0) {
            active_set = TRUE;
            active_value = TRUE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--inactive") == 0) {
            active_set = TRUE;
            active_value = FALSE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--monitor") == 0) {
            monitor_set = TRUE;
            monitor_value = TRUE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--no-monitor") == 0) {
            monitor_set = TRUE;
            monitor_value = FALSE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--one-file-system") == 0) {
            onefs_set = TRUE;
            onefs_value = TRUE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--no-one-file-system") == 0) {
            onefs_set = TRUE;
            onefs_value = FALSE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--scan-after-launch") == 0) {
            scan_set = TRUE;
            scan_value = TRUE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--no-scan-after-launch") == 0) {
            scan_set = TRUE;
            scan_value = FALSE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--rescan-every") == 0) {
            if (++i >= argc) {
                fsearch_cli_printerr("fsearch config roots: --rescan-every requires SECONDS\n");
                return 2;
            }
            rescan_value = argv[i];
        }
        else if (!positional_only && strcmp(argv[i], "--rescan") == 0) {
            rescan_now = TRUE;
        }
        else if (!positional_only && argv[i][0] == '-') {
            return unknown_option("roots", argv[i]);
        }
        else if (!path_arg) {
            path_arg = argv[i];
        }
        else {
            return unexpected_argument("roots", argv[i]);
        }
    }
    if (!path_arg) {
        fsearch_cli_printerr("fsearch config roots: missing PATH\n");
        return 2;
    }
    g_autofree char *path = g_canonicalize_filename(path_arg, NULL);
    g_autoptr(FsearchDatabaseInclude) old = find_root(config, path);
    if (remove) {
        if (!old) {
            fsearch_cli_printerr("fsearch config roots: path not found: %s\n", path);
            return 2;
        }
        fsearch_database_include_manager_remove(config->includes, old);
    }
    else {
        if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
            fsearch_cli_printerr("fsearch config roots: not a directory: %s\n", path);
            return 2;
        }
        if (add && old) {
            fsearch_cli_printerr("fsearch config roots: path already exists: %s\n", path);
            return 2;
        }
        if (set && !old) {
            fsearch_cli_printerr("fsearch config roots: path not found: %s\n", path);
            return 2;
        }
        gboolean active = old ? fsearch_database_include_get_active(old) : TRUE;
        gboolean monitor = old ? fsearch_database_include_get_monitored(old) : FALSE;
        gboolean onefs = old ? fsearch_database_include_get_one_file_system(old) : FALSE;
        gboolean scan = old ? fsearch_database_include_get_scan_after_launch(old) : FALSE;
        gint64 rescan = old ? fsearch_database_include_get_rescan_after(old) : 0;
        if (active_set)
            active = active_value;
        if (monitor_set)
            monitor = monitor_value;
        if (onefs_set)
            onefs = onefs_value;
        if (scan_set)
            scan = scan_value;
        if (rescan_value) {
            char *end = NULL;
            rescan = g_ascii_strtoll(rescan_value, &end, 10);
            if (!end || *end || rescan < 0) {
                fsearch_cli_printerr("fsearch config roots: invalid rescan duration: %s\n", rescan_value);
                return 2;
            }
        }
        if (old)
            fsearch_database_include_manager_remove(config->includes, old);
        g_autoptr(FsearchDatabaseInclude) root = fsearch_database_include_new(path, active, onefs, monitor, scan, rescan);
        fsearch_database_include_manager_add(config->includes, root);
    }
    if (!save_config(config))
        return 1;
    g_autoptr(FsearchDatabaseInclude) saved = find_root(config, path);
    if (saved) {
        fsearch_cli_printerr("%s\tactive=%s monitor=%s one-file-system=%s scan-after-launch=%s rescan-after=%" G_GINT64_FORMAT "\n",
                   path,
                   fsearch_database_include_get_active(saved) ? "true" : "false",
                   fsearch_database_include_get_monitored(saved) ? "true" : "false",
                   fsearch_database_include_get_one_file_system(saved) ? "true" : "false",
                   fsearch_database_include_get_scan_after_launch(saved) ? "true" : "false",
                   fsearch_database_include_get_rescan_after(saved));
    }
    else {
        fsearch_cli_printerr("%s\n", path);
    }
    return finish_index_mutation(rescan_now);
}

static int
run_excludes(FsearchConfig *config, int argc, char *argv[]) {
    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_excludes_help();
        return argc < 3 ? 2 : 0;
    }
    if (strcmp(argv[2], "list") == 0) {
        gboolean json = FALSE;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--json") == 0)
                json = TRUE;
            else if (argv[i][0] == '-')
                return unknown_option("excludes list", argv[i]);
            else
                return unexpected_argument("excludes list", argv[i]);
        }
        g_autoptr(GPtrArray) excludes = fsearch_database_exclude_manager_get_excludes(config->excludes);
        if (json)
            g_print("[");
        for (guint i = 0; i < excludes->len; i++) {
            FsearchDatabaseExclude *e = g_ptr_array_index(excludes, i);
            if (json) {
                g_autofree char *pattern = json_quote(fsearch_database_exclude_get_pattern(e));
                g_print("%s{\"pattern\":%s,\"active\":%s,\"type\":\"%s\",\"scope\":\"%s\",\"target\":\"%s\"}",
                        i ? "," : "",
                        pattern,
                        fsearch_database_exclude_get_active(e) ? "true" : "false",
                        fsearch_database_exclude_type_to_string(fsearch_database_exclude_get_exclude_type(e)),
                        fsearch_database_exclude_match_scope_to_string(fsearch_database_exclude_get_match_scope(e)),
                        fsearch_database_exclude_target_to_string(fsearch_database_exclude_get_target(e)));
            }
            else
                g_print("%s\tactive=%s type=%s scope=%s target=%s\n",
                        fsearch_database_exclude_get_pattern(e),
                        fsearch_database_exclude_get_active(e) ? "true" : "false",
                        fsearch_database_exclude_type_to_string(fsearch_database_exclude_get_exclude_type(e)),
                        fsearch_database_exclude_match_scope_to_string(fsearch_database_exclude_get_match_scope(e)),
                        fsearch_database_exclude_target_to_string(fsearch_database_exclude_get_target(e)));
        }
        if (json)
            g_print("]\n");
        return 0;
    }
    if (strcmp(argv[2], "hidden") == 0) {
        const char *value = NULL;
        gboolean positional_only = FALSE;
        gboolean rescan_now = FALSE;
        for (int i = 3; i < argc; i++) {
            if (!positional_only && strcmp(argv[i], "--") == 0)
                positional_only = TRUE;
            else if (!positional_only && strcmp(argv[i], "--rescan") == 0)
                rescan_now = TRUE;
            else if (!positional_only && argv[i][0] == '-')
                return unknown_option("excludes hidden", argv[i]);
            else if (!value)
                value = argv[i];
            else
                return unexpected_argument("excludes hidden", argv[i]);
        }
        if (!value) {
            g_print("%s\n", fsearch_database_exclude_manager_get_exclude_hidden(config->excludes) ? "true" : "false");
            return 0;
        }
        bool hidden;
        if (!parse_bool(value, &hidden)) {
            fsearch_cli_printerr("fsearch config excludes hidden: expected true or false\n");
            return 2;
        }
        fsearch_database_exclude_manager_set_exclude_hidden(config->excludes, hidden);
        if (!save_config(config))
            return 1;
        fsearch_cli_printerr("exclude-hidden = %s\n", hidden ? "true" : "false");
        return finish_index_mutation(rescan_now);
    }
    const gboolean add = strcmp(argv[2], "add") == 0;
    const gboolean remove = strcmp(argv[2], "remove") == 0;
    if (!add && !remove) {
        fsearch_cli_printerr("fsearch config excludes: unknown operation: %s\n", argv[2]);
        return 2;
    }

    const char *pattern = NULL;
    const char *type_str = NULL;
    const char *scope_str = NULL;
    const char *target_str = NULL;
    gboolean active_set = FALSE;
    gboolean active = TRUE;
    gboolean positional_only = FALSE;
    gboolean rescan_now = FALSE;
    for (int i = 3; i < argc; i++) {
        if (!positional_only && strcmp(argv[i], "--") == 0) {
            positional_only = TRUE;
        }
        else if (!positional_only
                 && (strcmp(argv[i], "--type") == 0 || strcmp(argv[i], "--scope") == 0
                     || strcmp(argv[i], "--target") == 0)) {
            const char *option = argv[i];
            if (++i >= argc) {
                fsearch_cli_printerr("fsearch config excludes: %s requires a value\n", option);
                return 2;
            }
            if (strcmp(option, "--type") == 0)
                type_str = argv[i];
            else if (strcmp(option, "--scope") == 0)
                scope_str = argv[i];
            else
                target_str = argv[i];
        }
        else if (!positional_only && strcmp(argv[i], "--active") == 0) {
            active_set = TRUE;
            active = TRUE;
        }
        else if (!positional_only && strcmp(argv[i], "--inactive") == 0) {
            active_set = TRUE;
            active = FALSE;
        }
        else if (!positional_only && strcmp(argv[i], "--rescan") == 0) {
            rescan_now = TRUE;
        }
        else if (!positional_only && argv[i][0] == '-') {
            return unknown_option("excludes", argv[i]);
        }
        else if (!pattern) {
            pattern = argv[i];
        }
        else {
            return unexpected_argument("excludes", argv[i]);
        }
    }
    if (!pattern) {
        fsearch_cli_printerr("fsearch config excludes: missing PATTERN\n");
        return 2;
    }
    if (type_str && strcmp(type_str, "fixed") != 0 && strcmp(type_str, "wildcard") != 0
        && strcmp(type_str, "regex") != 0) {
        fsearch_cli_printerr("fsearch config excludes: invalid type: %s\n", type_str);
        return 2;
    }
    if (scope_str && strcmp(scope_str, "full-path") != 0 && strcmp(scope_str, "full_path") != 0
        && strcmp(scope_str, "basename") != 0) {
        fsearch_cli_printerr("fsearch config excludes: invalid scope: %s\n", scope_str);
        return 2;
    }
    if (target_str && strcmp(target_str, "both") != 0 && strcmp(target_str, "files") != 0
        && strcmp(target_str, "folders") != 0) {
        fsearch_cli_printerr("fsearch config excludes: invalid target: %s\n", target_str);
        return 2;
    }
    if (scope_str && strcmp(scope_str, "full-path") == 0)
        scope_str = "full_path";
    FsearchDatabaseExcludeType type = fsearch_database_exclude_get_type_from_string(type_str ? type_str : "fixed");
    FsearchDatabaseExcludeMatchScope scope = fsearch_database_exclude_get_match_scope_from_string(scope_str ? scope_str
                                                                                                            : "full_"
                                                                                                              "path");
    FsearchDatabaseExcludeTarget target = fsearch_database_exclude_get_target_from_string(target_str ? target_str
                                                                                                     : "both");
    g_autoptr(FsearchDatabaseExclude) requested = fsearch_database_exclude_new(pattern, active, type, scope, target);
    if (add) {
        fsearch_database_exclude_manager_add(config->excludes, requested);
    }
    else {
        g_autoptr(GPtrArray) excludes = fsearch_database_exclude_manager_get_excludes(config->excludes);
        FsearchDatabaseExclude *found = NULL;
        for (guint i = 0; i < excludes->len; i++) {
            FsearchDatabaseExclude *candidate = g_ptr_array_index(excludes, i);
            if (g_strcmp0(fsearch_database_exclude_get_pattern(candidate), pattern) == 0
                && fsearch_database_exclude_get_exclude_type(candidate) == type
                && fsearch_database_exclude_get_match_scope(candidate) == scope
                && fsearch_database_exclude_get_target(candidate) == target
                && (!active_set || fsearch_database_exclude_get_active(candidate) == active)) {
                found = candidate;
                break;
            }
        }
        if (!found) {
            fsearch_cli_printerr("fsearch config excludes: exclusion not found: %s\n", pattern);
            return 2;
        }
        fsearch_database_exclude_manager_remove(config->excludes, found);
    }
    if (!save_config(config))
        return 1;
    fsearch_cli_printerr("%s\n", pattern);
    return finish_index_mutation(rescan_now);
}

static int
run_filters(FsearchConfig *config, int argc, char *argv[]) {
    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_filters_help();
        return argc < 3 ? 2 : 0;
    }
    if (strcmp(argv[2], "list") == 0) {
        gboolean json = FALSE;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--json") == 0)
                json = TRUE;
            else if (argv[i][0] == '-')
                return unknown_option("filters list", argv[i]);
            else
                return unexpected_argument("filters list", argv[i]);
        }
        if (json)
            g_print("[");
        const guint count = fsearch_filter_manager_get_num_filters(config->filters);
        for (guint i = 0; i < count; i++) {
            g_autoptr(FsearchFilter) filter = fsearch_filter_manager_get_filter(config->filters, i);
            if (json) {
                g_autofree char *name = json_quote(filter->name);
                g_autofree char *query = json_quote(filter->query);
                g_autofree char *macro = json_quote(filter->macro);
                g_print("%s{\"name\":%s,\"query\":%s,\"macro\":%s,\"case\":%s,\"path\":%s,\"regex\":%s}",
                        i ? "," : "",
                        name,
                        query,
                        macro,
                        filter->flags & QUERY_FLAG_MATCH_CASE ? "true" : "false",
                        filter->flags & QUERY_FLAG_SEARCH_IN_PATH ? "true" : "false",
                        filter->flags & QUERY_FLAG_REGEX ? "true" : "false");
            }
            else
                g_print("%s\tmacro=%s query=%s case=%s path=%s regex=%s\n",
                        filter->name,
                        filter->macro,
                        filter->query,
                        filter->flags & QUERY_FLAG_MATCH_CASE ? "true" : "false",
                        filter->flags & QUERY_FLAG_SEARCH_IN_PATH ? "true" : "false",
                        filter->flags & QUERY_FLAG_REGEX ? "true" : "false");
        }
        if (json)
            g_print("]\n");
        return 0;
    }
    if (strcmp(argv[2], "reset") == 0) {
        if (argc > 3) {
            if (argv[3][0] == '-')
                return unknown_option("filters reset", argv[3]);
            return unexpected_argument("filters reset", argv[3]);
        }
        g_clear_pointer(&config->filters, fsearch_filter_manager_unref);
        config->filters = fsearch_filter_manager_new_with_defaults();
        if (!save_config(config))
            return 1;
        fsearch_cli_printerr("filters reset\n");
        return 0;
    }
    const gboolean add = strcmp(argv[2], "add") == 0;
    const gboolean set = strcmp(argv[2], "set") == 0;
    const gboolean remove = strcmp(argv[2], "remove") == 0;
    if (!add && !set && !remove) {
        fsearch_cli_printerr("fsearch config filters: unknown operation: %s\n", argv[2]);
        return 2;
    }

    const char *name = NULL;
    const char *query = NULL;
    const char *macro = NULL;
    gboolean case_set = FALSE;
    gboolean case_value = FALSE;
    gboolean path_set = FALSE;
    gboolean path_value = FALSE;
    gboolean regex_set = FALSE;
    gboolean regex_value = FALSE;
    gboolean positional_only = FALSE;
    for (int i = 3; i < argc; i++) {
        if (!positional_only && strcmp(argv[i], "--") == 0) {
            positional_only = TRUE;
        }
        else if (!positional_only && (add || set)
                 && (strcmp(argv[i], "--query") == 0 || strcmp(argv[i], "--macro") == 0)) {
            const char *option = argv[i];
            if (++i >= argc) {
                fsearch_cli_printerr("fsearch config filters: %s requires a value\n", option);
                return 2;
            }
            if (strcmp(option, "--query") == 0)
                query = argv[i];
            else
                macro = argv[i];
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--case") == 0) {
            case_set = TRUE;
            case_value = TRUE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--no-case") == 0) {
            case_set = TRUE;
            case_value = FALSE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--path") == 0) {
            path_set = TRUE;
            path_value = TRUE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--no-path") == 0) {
            path_set = TRUE;
            path_value = FALSE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--regex") == 0) {
            regex_set = TRUE;
            regex_value = TRUE;
        }
        else if (!positional_only && (add || set) && strcmp(argv[i], "--no-regex") == 0) {
            regex_set = TRUE;
            regex_value = FALSE;
        }
        else if (!positional_only && argv[i][0] == '-') {
            return unknown_option("filters", argv[i]);
        }
        else if (!name) {
            name = argv[i];
        }
        else {
            return unexpected_argument("filters", argv[i]);
        }
    }
    if (!name) {
        fsearch_cli_printerr("fsearch config filters: missing NAME\n");
        return 2;
    }
    g_autoptr(FsearchFilter) existing = fsearch_filter_manager_get_filter_for_name(config->filters, name);
    if (remove) {
        if (!existing) {
            fsearch_cli_printerr("fsearch config filters: name not found: %s\n", name);
            return 2;
        }
        fsearch_filter_manager_remove(config->filters, existing);
    }
    else {
        if (add && existing) {
            fsearch_cli_printerr("fsearch config filters: name already exists: %s\n", name);
            return 2;
        }
        if (set && !existing) {
            fsearch_cli_printerr("fsearch config filters: name not found: %s\n", name);
            return 2;
        }
        FsearchQueryFlags flags = 0;
        if (existing)
            flags = existing->flags;
        if (case_set && case_value)
            flags |= QUERY_FLAG_MATCH_CASE;
        if (case_set && !case_value)
            flags &= ~QUERY_FLAG_MATCH_CASE;
        if (path_set && path_value)
            flags |= QUERY_FLAG_SEARCH_IN_PATH;
        if (path_set && !path_value)
            flags &= ~QUERY_FLAG_SEARCH_IN_PATH;
        if (regex_set && regex_value)
            flags |= QUERY_FLAG_REGEX;
        if (regex_set && !regex_value)
            flags &= ~QUERY_FLAG_REGEX;
        if (existing)
            fsearch_filter_manager_edit(config->filters,
                                        existing,
                                        name,
                                        macro ? macro : existing->macro,
                                        query ? query : existing->query,
                                        flags);
        else {
            g_autoptr(FsearchFilter) filter = fsearch_filter_new(name, macro, query, flags);
            fsearch_filter_manager_append_filter(config->filters, filter);
        }
    }
    if (!save_config(config))
        return 1;
    g_autoptr(FsearchFilter) saved = fsearch_filter_manager_get_filter_for_name(config->filters, name);
    if (saved)
        fsearch_cli_printerr("%s\tmacro=%s query=%s\n", saved->name, saved->macro, saved->query);
    else
        fsearch_cli_printerr("%s\n", name);
    return 0;
}

static gboolean
reset_scalar_section(FsearchConfig *config, FsearchConfig *defaults, const char *target) {
    gboolean matched = FALSE;
    for (guint i = 0; i < G_N_ELEMENTS(scalar_settings); i++) {
        const ScalarSetting *setting = &scalar_settings[i];
        const gboolean exact = strcmp(setting->key, target) == 0;
        g_autofree char *prefix = g_strdup_printf("%s.", target);
        if (!exact && !g_str_has_prefix(setting->key, prefix))
            continue;
        copy_scalar_value(setting, config, defaults);
        matched = TRUE;
    }
    return matched;
}

static int
run_reset(FsearchConfig *config, int argc, char *argv[]) {
    gboolean confirmed = FALSE;
    gboolean rescan_now = FALSE;
    gboolean positional_only = FALSE;
    const char *target = NULL;
    for (int i = 2; i < argc; i++) {
        if (!positional_only && strcmp(argv[i], "--") == 0) {
            positional_only = TRUE;
        }
        else if (!positional_only && strcmp(argv[i], "--yes") == 0) {
            confirmed = TRUE;
        }
        else if (!positional_only && strcmp(argv[i], "--rescan") == 0) {
            rescan_now = TRUE;
        }
        else if (!positional_only && argv[i][0] == '-') {
            return unknown_option("reset", argv[i]);
        }
        else if (!target) {
            target = argv[i];
        }
        else {
            return unexpected_argument("reset", argv[i]);
        }
    }
    if (!confirmed) {
        fsearch_cli_printerr("fsearch config reset requires --yes\n");
        return 2;
    }
    if (!target)
        target = "all";
    const gboolean index_target = strcmp(target, "all") == 0 || strcmp(target, "roots") == 0
                               || strcmp(target, "database") == 0 || strcmp(target, "excludes") == 0;
    if (rescan_now && !index_target) {
        fsearch_cli_printerr("fsearch config reset: --rescan requires roots, database, excludes, or all\n");
        return 2;
    }
    g_autoptr(FsearchConfig) defaults = g_new0(FsearchConfig, 1);
    config_load_default(defaults);
    gboolean matched = FALSE;
    if (strcmp(target, "all") == 0) {
        for (guint i = 0; i < G_N_ELEMENTS(scalar_settings); i++) {
            copy_scalar_value(&scalar_settings[i], config, defaults);
        }
        g_clear_object(&config->includes);
        config->includes = fsearch_database_include_manager_copy(defaults->includes);
        g_clear_object(&config->excludes);
        config->excludes = fsearch_database_exclude_manager_copy(defaults->excludes);
        g_clear_pointer(&config->filters, fsearch_filter_manager_unref);
        config->filters = fsearch_filter_manager_copy(defaults->filters);
        matched = TRUE;
    }
    else if (strcmp(target, "roots") == 0 || strcmp(target, "database") == 0) {
        g_clear_object(&config->includes);
        config->includes = fsearch_database_include_manager_copy(defaults->includes);
        if (strcmp(target, "database") == 0) {
            g_clear_object(&config->excludes);
            config->excludes = fsearch_database_exclude_manager_copy(defaults->excludes);
        }
        matched = TRUE;
    }
    else if (strcmp(target, "excludes") == 0) {
        g_clear_object(&config->excludes);
        config->excludes = fsearch_database_exclude_manager_copy(defaults->excludes);
        matched = TRUE;
    }
    else if (strcmp(target, "filters") == 0) {
        g_clear_pointer(&config->filters, fsearch_filter_manager_unref);
        config->filters = fsearch_filter_manager_copy(defaults->filters);
        matched = TRUE;
    }
    else
        matched = reset_scalar_section(config, defaults, target);
    if (!matched) {
        fsearch_cli_printerr("fsearch config: unknown setting or section: %s\n", target);
        return 2;
    }
    if (!save_config(config))
        return 1;
    fsearch_cli_printerr("%s settings reset\n", target);
    return index_target ? finish_index_mutation(rescan_now) : 0;
}

static int
run_doctor(FsearchConfig *config, gboolean config_existed) {
    if (!config_existed) {
        g_print("%s", _("configuration: missing; defaults active\nindex: unknown\n"));
        return 1;
    }
    g_autofree char *database_path = g_build_filename(g_get_user_data_dir(), "fsearch", "fsearch.db", NULL);
    g_autoptr(FsearchDatabaseIncludeManager) indexed_includes = NULL;
    g_autoptr(FsearchDatabaseExcludeManager) indexed_excludes = NULL;
    FsearchDatabaseIndexPropertyFlags flags = DATABASE_INDEX_PROPERTY_FLAG_NONE;
    if (!fsearch_database_file_load_config(database_path, &indexed_includes, &indexed_excludes, &flags)) {
        g_print("%s", _("configuration: ok\nindex: missing or unreadable\n"));
        return 1;
    }
    const gboolean current = fsearch_database_include_manager_equal(config->includes, indexed_includes)
                          && fsearch_database_exclude_manager_equal(config->excludes, indexed_excludes);
    g_print(_("configuration: ok\nindex: %s\n"), current ? _("current") : _("stale"));
    return current ? 0 : 1;
}

int
fsearch_cli_config_run(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "help") == 0) {
        print_help();
        return 0;
    }
    if (strcmp(argv[1], "path") == 0) {
        if (argc > 2 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
            g_print("%s", _("Usage: fsearch config path\nPrint the active FSearch configuration file path.\n"));
            return 0;
        }
        if (argc > 2) {
            if (argv[2][0] == '-')
                return unknown_option("path", argv[2]);
            return unexpected_argument("path", argv[2]);
        }
        g_autofree char *path = config_path();
        g_print("%s\n", path);
        return 0;
    }
    if (command_help_requested(argc, argv)
        && (strcmp(argv[1], "show") == 0 || strcmp(argv[1], "get") == 0 || strcmp(argv[1], "set") == 0
            || strcmp(argv[1], "unset") == 0 || strcmp(argv[1], "reset") == 0 || strcmp(argv[1], "doctor") == 0)) {
        print_scalar_command_help(argv[1]);
        return 0;
    }
    if (strcmp(argv[1], "doctor") == 0 && argc > 2) {
        if (argv[2][0] == '-')
            return unknown_option("doctor", argv[2]);
        return unexpected_argument("doctor", argv[2]);
    }
    gboolean existed = FALSE;
    g_autoptr(FsearchConfig) config = load_config(&existed);
    if (strcmp(argv[1], "show") == 0)
        return run_show(config, argc, argv);
    if (strcmp(argv[1], "get") == 0)
        return run_get(config, argc, argv);
    if (strcmp(argv[1], "set") == 0)
        return run_set(config, argc, argv, FALSE);
    if (strcmp(argv[1], "unset") == 0)
        return run_set(config, argc, argv, TRUE);
    if (strcmp(argv[1], "reset") == 0)
        return run_reset(config, argc, argv);
    if (strcmp(argv[1], "roots") == 0)
        return run_roots(config, argc, argv);
    if (strcmp(argv[1], "excludes") == 0)
        return run_excludes(config, argc, argv);
    if (strcmp(argv[1], "filters") == 0)
        return run_filters(config, argc, argv);
    if (strcmp(argv[1], "doctor") == 0)
        return run_doctor(config, existed);
    fsearch_cli_printerr("fsearch config: unknown command: %s\n", argv[1]);
    return 2;
}
