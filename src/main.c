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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fsearch.h"
#include "fsearch_cli.h"
#include "fsearch_locale.h"
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <locale.h>

int
main(int argc, char *argv[]) {
    FsearchLocaleArguments locale_arguments = {0};
    g_autoptr(GError) locale_error = NULL;
    if (!fsearch_locale_arguments_parse(argc,
                                        argv,
                                        g_getenv("FSEARCH_LANG"),
                                        &locale_arguments,
                                        &locale_error)) {
        g_printerr("fsearch: %s\n", locale_error->message);
        return EXIT_FAILURE;
    }

    setlocale(LC_ALL, "");
    if (!fsearch_locale_activate(locale_arguments.language, &locale_error)) {
        g_printerr("fsearch: %s\n", locale_error->message);
        fsearch_locale_arguments_clear(&locale_arguments);
        return EXIT_FAILURE;
    }
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    g_set_application_name(_("FSearch"));
    g_set_prgname("io.github.cboxdoerfer.FSearch");

    if (fsearch_cli_select_frontend(locale_arguments.argc,
                                    (const char *const *)locale_arguments.argv,
                                    g_getenv("FSEARCH_UI")) == FSEARCH_CLI_FRONTEND_CLI) {
        const int result = fsearch_cli_run(locale_arguments.argc, locale_arguments.argv);
        fsearch_locale_arguments_clear(&locale_arguments);
        return result;
    }

    char **gui_argv = g_new0(char *, locale_arguments.argc + 1);
    int gui_argc = 0;
    for (int i = 0; i < locale_arguments.argc; i++) {
        if (g_strcmp0(locale_arguments.argv[i], "--cli") != 0 && g_strcmp0(locale_arguments.argv[i], "--gui") != 0) {
            gui_argv[gui_argc++] = locale_arguments.argv[i];
        }
    }
    const int result = g_application_run(G_APPLICATION(fsearch_application_new()), gui_argc, gui_argv);
    g_free(gui_argv);
    fsearch_locale_arguments_clear(&locale_arguments);
    return result;
}
