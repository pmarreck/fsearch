#include "fsearch_glob.h"

#include <errno.h>
#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define BASE_SEGMENT "src/{AGENTS,CLAUDE}/[a-z]?/"
#define BASE_SEGMENT_COUNT 2048
#define CONVERSIONS_PER_SAMPLE 32

static guint
parse_multiplier(const char *argument) {
    char *end = NULL;
    errno = 0;
    const guint64 value = g_ascii_strtoull(argument, &end, 10);
    if (errno || end == argument || *end != '\0' || value > G_MAXUINT) {
        return 0;
    }
    return (guint)value;
}

int
main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        return EXIT_FAILURE;
    }

    const guint multiplier = parse_multiplier(argv[1]);
    if (multiplier != 1 && multiplier != 2 && multiplier != 4 && multiplier != 8) {
        return EXIT_FAILURE;
    }
    const bool show_metadata = argc == 3 && strcmp(argv[2], "--metadata") == 0;
    if (argc == 3 && !show_metadata) {
        return EXIT_FAILURE;
    }

    GString *glob = g_string_sized_new(sizeof(BASE_SEGMENT) * BASE_SEGMENT_COUNT * multiplier);
    for (guint i = 0; i < BASE_SEGMENT_COUNT * multiplier; ++i) {
        g_string_append(glob, BASE_SEGMENT);
    }
    g_string_append(glob, "target.md");

    if (show_metadata) {
        g_print("%" G_GSIZE_FORMAT "\t%u\n", glob->len, CONVERSIONS_PER_SAMPLE);
        g_string_free(glob, TRUE);
        return EXIT_SUCCESS;
    }

    volatile size_t checksum = 0;
    for (guint i = 0; i < CONVERSIONS_PER_SAMPLE; ++i) {
        g_autofree char *regex = fsearch_glob_to_regex(glob->str);
        if (!regex) {
            g_string_free(glob, TRUE);
            return EXIT_FAILURE;
        }
        checksum += strlen(regex);
    }
    g_string_free(glob, TRUE);
    return checksum == 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
