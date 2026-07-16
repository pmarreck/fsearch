#include "fsearch_locale.h"

#include <locale.h>
#include <string.h>

GQuark
fsearch_locale_error_quark(void) {
    return g_quark_from_static_string("fsearch-locale-error");
}

static char *
normalize_language_code(const char *value, GError **error) {
    if (!value || *value == '\0') {
        g_set_error(error,
                    FSEARCH_LOCALE_ERROR,
                    FSEARCH_LOCALE_ERROR_INVALID_VALUE,
                    "invalid language code: %s",
                    value ? value : "");
        return NULL;
    }

    g_auto(GStrv) parts = g_strsplit_set(value, "-_", -1);
    if (!parts[0] || (strlen(parts[0]) != 2 && strlen(parts[0]) != 3)) {
        goto invalid;
    }

    for (guint i = 0; parts[i]; i++) {
        const gsize length = strlen(parts[i]);
        if (length == 0 || length > 8) {
            goto invalid;
        }
        for (gsize character = 0; character < length; character++) {
            if (!g_ascii_isalnum(parts[i][character])) {
                goto invalid;
            }
            if (i == 0 && !g_ascii_isalpha(parts[i][character])) {
                goto invalid;
            }
        }
    }

    GString *normalized = g_string_new(NULL);
    for (guint i = 0; parts[i]; i++) {
        if (i > 0) {
            g_string_append_c(normalized, '_');
        }

        const gsize length = strlen(parts[i]);
        if (i == 0) {
            g_autofree char *lower = g_ascii_strdown(parts[i], -1);
            g_string_append(normalized, lower);
        }
        else if (length == 2) {
            g_autofree char *upper = g_ascii_strup(parts[i], -1);
            g_string_append(normalized, upper);
        }
        else if (length == 4) {
            g_string_append_c(normalized, g_ascii_toupper(parts[i][0]));
            g_autofree char *lower = g_ascii_strdown(parts[i] + 1, -1);
            g_string_append(normalized, lower);
        }
        else {
            g_autofree char *lower = g_ascii_strdown(parts[i], -1);
            g_string_append(normalized, lower);
        }
    }
    char *code = g_string_free(normalized, FALSE);
    if (g_strcmp0(code, "zh") == 0 ||
        g_strcmp0(code, "zh_CN") == 0 ||
        g_strcmp0(code, "zh_SG") == 0 ||
        g_strcmp0(code, "zh_MY") == 0 ||
        g_strcmp0(code, "zh_Hans") == 0 ||
        g_str_has_prefix(code, "zh_Hans_")) {
        g_free(code);
        return g_strdup("zh_CN");
    }
    if (g_strcmp0(code, "zh_TW") == 0 ||
        g_strcmp0(code, "zh_HK") == 0 ||
        g_strcmp0(code, "zh_MO") == 0 ||
        g_strcmp0(code, "zh_Hant") == 0 ||
        g_str_has_prefix(code, "zh_Hant_")) {
        g_free(code);
        return g_strdup("zh_Hant");
    }
    return code;

invalid:
    g_set_error(error,
                FSEARCH_LOCALE_ERROR,
                FSEARCH_LOCALE_ERROR_INVALID_VALUE,
                "invalid language code: %s",
                value);
    return NULL;
}

gboolean
fsearch_locale_arguments_parse(int argc,
                               char *const argv[],
                               const char *environment_language,
                               FsearchLocaleArguments *arguments,
                               GError **error) {
    g_return_val_if_fail(argc > 0, FALSE);
    g_return_val_if_fail(argv != NULL, FALSE);
    g_return_val_if_fail(arguments != NULL, FALSE);

    *arguments = (FsearchLocaleArguments){0};
    arguments->argv = g_new0(char *, argc + 1);
    arguments->argv[arguments->argc++] = argv[0];

    const char *selected_language = environment_language && *environment_language ? environment_language : NULL;
    gboolean parse_options = TRUE;
    for (int i = 1; i < argc; i++) {
        if (parse_options && g_strcmp0(argv[i], "--") == 0) {
            parse_options = FALSE;
            arguments->argv[arguments->argc++] = argv[i];
            continue;
        }

        const char *argument_language = NULL;
        if (parse_options && g_strcmp0(argv[i], "--lang") == 0) {
            if (i + 1 >= argc || g_str_has_prefix(argv[i + 1], "--")) {
                g_set_error_literal(error,
                                    FSEARCH_LOCALE_ERROR,
                                    FSEARCH_LOCALE_ERROR_MISSING_VALUE,
                                    "--lang requires a language code");
                fsearch_locale_arguments_clear(arguments);
                return FALSE;
            }
            argument_language = argv[++i];
        }
        else if (parse_options && g_str_has_prefix(argv[i], "--lang=")) {
            argument_language = argv[i] + strlen("--lang=");
        }

        if (argument_language) {
            selected_language = argument_language;
            continue;
        }
        arguments->argv[arguments->argc++] = argv[i];
    }

    if (selected_language) {
        arguments->language = normalize_language_code(selected_language, error);
        if (!arguments->language) {
            fsearch_locale_arguments_clear(arguments);
            return FALSE;
        }
    }
    return TRUE;
}

void
fsearch_locale_arguments_clear(FsearchLocaleArguments *arguments) {
    if (!arguments) {
        return;
    }
    g_clear_pointer(&arguments->argv, g_free);
    g_clear_pointer(&arguments->language, g_free);
    arguments->argc = 0;
}

static gboolean
locale_is_c(const char *locale) {
    return !locale ||
           g_ascii_strcasecmp(locale, "C") == 0 ||
           g_ascii_strcasecmp(locale, "POSIX") == 0 ||
           g_ascii_strcasecmp(locale, "C.UTF-8") == 0 ||
           g_ascii_strcasecmp(locale, "C.utf8") == 0;
}

gboolean
fsearch_locale_activate(const char *language, GError **error) {
    if (!language) {
        return TRUE;
    }

    g_setenv("LANGUAGE", language, TRUE);
    if (!locale_is_c(setlocale(LC_MESSAGES, NULL))) {
        return TRUE;
    }

    g_autofree char *utf8_locale = g_strdup_printf("%s.UTF-8", language);
    g_autofree char *utf8_short_locale = g_strdup_printf("%s.utf8", language);
    const char *const candidates[] = {
        language,
        utf8_locale,
        utf8_short_locale,
        "en_US.UTF-8",
        "en_US.utf8",
        "en_US",
#ifdef G_OS_WIN32
        "English_United States.1252",
#endif
        NULL,
    };
    for (guint i = 0; candidates[i]; i++) {
        if (setlocale(LC_MESSAGES, candidates[i])) {
            return TRUE;
        }
    }

    g_set_error(error,
                FSEARCH_LOCALE_ERROR,
                FSEARCH_LOCALE_ERROR_UNAVAILABLE,
                "cannot activate language code: %s",
                language);
    return FALSE;
}
