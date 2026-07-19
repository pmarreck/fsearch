test_array = executable('test_array', 'test_array.c', dependencies: libfsearch_dep)
test_query = executable('test_query', 'test_query.c', dependencies: libfsearch_dep)
test_size_utils = executable('test_size_utils', 'test_size_utils.c', dependencies: libfsearch_dep)
test_string_utils = executable('test_string_utils', 'test_string_utils.c', dependencies: libfsearch_dep)
test_time_utils = executable('test_time_utils', 'test_time_utils.c', dependencies: libfsearch_dep)
test_database_entry = executable('test_database_entry', 'test_database_entry.c', dependencies: libfsearch_dep)
test_database_include = executable('test_database_include', 'test_database_include.c', dependencies : libfsearch_dep)
test_database_exclude = executable('test_database_exclude', 'test_database_exclude.c', dependencies : libfsearch_dep)
test_database_chunked_array = executable('test_database_chunked_array', 'test_database_chunked_array.c', dependencies : libfsearch_dep)
test_database_index_store = executable('test_database_index_store', 'test_database_index_store.c', dependencies : libfsearch_dep)
test_database = executable('test_database', 'test_database.c', dependencies : libfsearch_dep)
test_database_file = executable('test_database_file', 'test_database_file.c', dependencies : libfsearch_dep)
test_cli = executable('test_cli', 'test_cli.c', dependencies : libfsearch_dep)
test_environment = executable('test_environment', 'test_environment.c', dependencies : libfsearch_dep)
benchmark_glob = executable('benchmark_glob', 'benchmark_glob.c', dependencies : libfsearch_dep)

fsearch_test_root = join_paths(meson.current_build_dir(), 'fsearch-test-state')
fsearch_test_environment = environment()
fsearch_test_environment.set('FSEARCH_TEST_ROOT', fsearch_test_root)
fsearch_test_environment.set('HOME', join_paths(fsearch_test_root, 'home'))
fsearch_test_environment.set('XDG_CONFIG_HOME', join_paths(fsearch_test_root, 'config'))
fsearch_test_environment.set('XDG_DATA_HOME', join_paths(fsearch_test_root, 'data'))
fsearch_test_environment.set('XDG_CACHE_HOME', join_paths(fsearch_test_root, 'cache'))
fsearch_test_environment.set('XDG_STATE_HOME', join_paths(fsearch_test_root, 'state'))
fsearch_test_environment.set('XDG_RUNTIME_DIR', join_paths(fsearch_test_root, 'runtime'))
fsearch_test_environment.set('G_TEST_SRCDIR', meson.current_source_dir())
fsearch_test_environment.set('G_TEST_BUILDDIR', meson.current_build_dir())

test('test_database',
     test_database,
     env : fsearch_test_environment,
     timeout : 60,
)
test('test_database_file',
     test_database_file,
     env : fsearch_test_environment,
)
test('test_database_chunked_array',
     test_database_chunked_array,
     env : fsearch_test_environment,
)
test('test_database_index_store',
     test_database_index_store,
     env : fsearch_test_environment,
)
test('test_database_include',
     test_database_include,
     env : fsearch_test_environment,
)
test('test_database_exclude',
     test_database_exclude,
     env : fsearch_test_environment,
)
test('test_database_entry',
     test_database_entry,
     env : fsearch_test_environment,
)
test('test_array',
     test_array,
     env : fsearch_test_environment,
)
test('test_query',
     test_query,
     env : fsearch_test_environment,
)
test('test_size_utils',
     test_size_utils,
     env : fsearch_test_environment,
)
test('test_string_utils',
     test_string_utils,
     env : fsearch_test_environment,
)
test('test_time_utils',
     test_time_utils,
     env : fsearch_test_environment,
)
test('test_cli', test_cli, env : fsearch_test_environment)
test('test_environment', test_environment, env : fsearch_test_environment)
