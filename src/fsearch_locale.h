#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    FSEARCH_LOCALE_ERROR_MISSING_VALUE,
    FSEARCH_LOCALE_ERROR_INVALID_VALUE,
    FSEARCH_LOCALE_ERROR_UNAVAILABLE,
} FsearchLocaleError;

#define FSEARCH_LOCALE_ERROR (fsearch_locale_error_quark())

typedef struct {
    int argc;
    char **argv;
    char *language;
} FsearchLocaleArguments;

GQuark
fsearch_locale_error_quark(void);

/// Parses the application-local locale selector before a frontend sees its arguments, so GUI and CLI share precedence.
gboolean
fsearch_locale_arguments_parse(int argc,
                               char *const argv[],
                               const char *environment_language,
                               FsearchLocaleArguments *arguments,
                               GError **error);

void
fsearch_locale_arguments_clear(FsearchLocaleArguments *arguments);

/// Activates an explicit GNU gettext catalog even when the invoking shell uses the non-translatable C locale.
gboolean
fsearch_locale_activate(const char *language, GError **error);

G_END_DECLS
