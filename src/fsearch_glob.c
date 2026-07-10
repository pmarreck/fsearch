/*
   FSearch - A fast file search utility
   Copyright © 2026 Christian Boxdörfer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "fsearch_glob.h"

#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FSEARCH_GLOB_MAX_NUMERIC_RANGE_VALUES 10000

static void
append_regex_literal(GString *regex, char character) {
    if (strchr(".[]^$+(){}|\\*?", character)) {
        g_string_append_c(regex, '\\');
    }
    g_string_append_c(regex, character);
}

static bool append_glob_to_regex(GString *regex, const char *glob, size_t length);

static bool
append_character_class(GString *regex, const char *glob, size_t length, size_t *position) {
    size_t current = *position + 1;
    GString *character_class = g_string_new("[");

    if (current < length && (glob[current] == '!' || glob[current] == '^')) {
        g_string_append_c(character_class, '^');
        ++current;
    }
    if (current < length && glob[current] == ']') {
        g_string_append_c(character_class, ']');
        ++current;
    }

    bool valid = false;
    for (; current < length; ++current) {
        const char character = glob[current];
        if (character == ']') {
            valid = true;
            ++current;
            break;
        }
        if (character == '\\' && current + 1 < length) {
            g_string_append_c(character_class, '\\');
            g_string_append_c(character_class, glob[++current]);
        }
        else {
            if (strchr("\\[]^", character)) {
                g_string_append_c(character_class, '\\');
            }
            g_string_append_c(character_class, character);
        }
    }

    if (!valid) {
        g_string_free(character_class, TRUE);
        return false;
    }

    g_string_append_c(character_class, ']');
    g_string_append(regex, character_class->str);
    g_string_free(character_class, TRUE);
    *position = current - 1;
    return true;
}

static bool
find_matching_brace(const char *glob, size_t length, size_t open_position, size_t *close_position) {
    size_t depth = 1;
    for (size_t current = open_position + 1; current < length; ++current) {
        if (glob[current] == '\\' && current + 1 < length) {
            ++current;
        }
        else if (glob[current] == '{') {
            ++depth;
        }
        else if (glob[current] == '}' && --depth == 0) {
            *close_position = current;
            return true;
        }
    }
    return false;
}

static bool
parse_numeric_range(const char *content, size_t length, gint64 *start, gint64 *end, size_t *width) {
    g_autofree char *range = g_strndup(content, length);
    char *separator = strstr(range, "..");
    if (!separator || strstr(separator + 2, "..")) {
        return false;
    }

    *separator = '\0';
    char *end_ptr = NULL;
    errno = 0;
    const gint64 parsed_start = g_ascii_strtoll(range, &end_ptr, 10);
    if (errno || end_ptr == range || *end_ptr != '\0') {
        return false;
    }

    char *range_end = separator + 2;
    errno = 0;
    const gint64 parsed_end = g_ascii_strtoll(range_end, &end_ptr, 10);
    if (errno || end_ptr == range_end || *end_ptr != '\0') {
        return false;
    }

    const char *start_digits = range + (range[0] == '-' || range[0] == '+' ? 1 : 0);
    const char *end_digits = range_end + (range_end[0] == '-' || range_end[0] == '+' ? 1 : 0);
    const bool preserve_padding = (strlen(start_digits) > 1 && start_digits[0] == '0')
                                  || (strlen(end_digits) > 1 && end_digits[0] == '0');
    *width = preserve_padding ? MAX(strlen(start_digits), strlen(end_digits)) : 0;
    *start = parsed_start;
    *end = parsed_end;
    return true;
}

static void
append_numeric_value(GString *regex, gint64 value, size_t width) {
    if (!width) {
        g_string_append_printf(regex, "%" G_GINT64_FORMAT, value);
        return;
    }

    if (value < 0) {
        g_string_append_c(regex, '-');
        value = -value;
    }
    g_string_append_printf(regex, "%0*" G_GINT64_FORMAT, (int)width, value);
}

static bool
append_numeric_range(GString *regex, const char *content, size_t length) {
    gint64 start = 0;
    gint64 end = 0;
    size_t width = 0;
    if (!parse_numeric_range(content, length, &start, &end, &width)) {
        return false;
    }

    GString *values = g_string_new("(?:");
    const gint64 increment = start <= end ? 1 : -1;
    gint64 value = start;
    for (uint32_t count = 0;; ++count) {
        if (count == FSEARCH_GLOB_MAX_NUMERIC_RANGE_VALUES || value == G_MININT64) {
            g_string_free(values, TRUE);
            return false;
        }
        if (count) {
            g_string_append_c(values, '|');
        }
        append_numeric_value(values, value, width);
        if (value == end) {
            break;
        }
        value += increment;
    }
    g_string_append_c(values, ')');
    g_string_append(regex, values->str);
    g_string_free(values, TRUE);
    return true;
}

static bool
append_brace_alternatives(GString *regex, const char *content, size_t length) {
    GString *alternatives = g_string_new("(?:");
    size_t start = 0;
    size_t depth = 0;
    bool has_alternative = false;

    for (size_t current = 0; current <= length; ++current) {
        if (current < length && content[current] == '\\' && current + 1 < length) {
            ++current;
            continue;
        }
        if (current < length && content[current] == '{') {
            ++depth;
            continue;
        }
        if (current < length && content[current] == '}') {
            if (!depth) {
                g_string_free(alternatives, TRUE);
                return false;
            }
            --depth;
            continue;
        }
        if ((current == length || content[current] == ',') && !depth) {
            if (current == start) {
                g_string_free(alternatives, TRUE);
                return false;
            }
            if (has_alternative) {
                g_string_append_c(alternatives, '|');
            }
            if (!append_glob_to_regex(alternatives, content + start, current - start)) {
                g_string_free(alternatives, TRUE);
                return false;
            }
            has_alternative = true;
            start = current + 1;
        }
    }

    if (!has_alternative) {
        g_string_free(alternatives, TRUE);
        return false;
    }
    g_string_append_c(alternatives, ')');
    g_string_append(regex, alternatives->str);
    g_string_free(alternatives, TRUE);
    return true;
}

static bool
append_brace_expression(GString *regex, const char *glob, size_t length, size_t *position) {
    size_t close_position = 0;
    if (!find_matching_brace(glob, length, *position, &close_position)) {
        return false;
    }

    const char *content = glob + *position + 1;
    const size_t content_length = close_position - *position - 1;
    GString *expression = g_string_new(NULL);
    const bool appended = append_numeric_range(expression, content, content_length)
                          || append_brace_alternatives(expression, content, content_length);
    if (appended) {
        g_string_append(regex, expression->str);
        *position = close_position;
    }
    g_string_free(expression, TRUE);
    return appended;
}

static bool
append_glob_to_regex(GString *regex, const char *glob, size_t length) {
    for (size_t position = 0; position < length; ++position) {
        switch (glob[position]) {
        case '\\':
            if (position + 1 < length) {
                append_regex_literal(regex, glob[++position]);
            }
            else {
                append_regex_literal(regex, '\\');
            }
            break;
        case '*':
            if (position + 1 < length && glob[position + 1] == '*') {
                if (position + 2 < length && glob[position + 2] == '/') {
                    g_string_append(regex, "(.*/)?");
                    position += 2;
                }
                else {
                    g_string_append(regex, ".*");
                    ++position;
                }
            }
            else {
                g_string_append(regex, "[^/]*");
            }
            break;
        case '?':
            g_string_append(regex, "[^/]");
            break;
        case '[':
            if (!append_character_class(regex, glob, length, &position)) {
                append_regex_literal(regex, glob[position]);
            }
            break;
        case '{':
            if (!append_brace_expression(regex, glob, length, &position)) {
                append_regex_literal(regex, glob[position]);
            }
            break;
        default:
            append_regex_literal(regex, glob[position]);
            break;
        }
    }
    return true;
}

char *
fsearch_glob_to_regex(const char *glob) {
    g_return_val_if_fail(glob, NULL);

    GString *regex = g_string_sized_new(strlen(glob) + 2);
    g_string_append_c(regex, '^');
    append_glob_to_regex(regex, glob, strlen(glob));
    g_string_append_c(regex, '$');
    return g_string_free(regex, FALSE);
}
