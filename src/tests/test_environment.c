#include <glib.h>

typedef struct {
    const char *name;
    const char *suffix;
} EnvironmentPath;

static void
test_xdg_paths_are_private(void) {
    static const EnvironmentPath paths[] = {
        {"HOME", "home"},
        {"XDG_CONFIG_HOME", "config"},
        {"XDG_DATA_HOME", "data"},
        {"XDG_CACHE_HOME", "cache"},
        {"XDG_STATE_HOME", "state"},
        {"XDG_RUNTIME_DIR", "runtime"},
    };

    const char *root = g_getenv("FSEARCH_TEST_ROOT");
    g_assert_nonnull(root);

    for (size_t i = 0; i < G_N_ELEMENTS(paths); i++) {
        g_autofree char *expected = g_build_filename(root, paths[i].suffix, NULL);
        g_assert_cmpstr(g_getenv(paths[i].name), ==, expected);
    }
}

int
main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/FSearch/environment/xdg_paths_are_private", test_xdg_paths_are_private);
    return g_test_run();
}
