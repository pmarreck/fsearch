# Plan

- [x] Establish a pinned Nix build, test, and run environment. Completed 2026-07-09 20:44 EDT.
  Verified the sandboxed flake package runs all existing Meson tests.
- [ ] Add top-level `build`, `test`, and `bm` commands and record benchmark results.
  Curiosity poke: benchmarks must use an optimized build and flag regressions without becoming flaky.
- [x] Define and test CLI selection, option precedence, and bounded-result rendering. Completed 2026-07-09 20:49 EDT.
  The pure selector gives later `--gui`/`--cli` flags precedence over `FSEARCH_UI`; limit resolution has default, environment, argument, and unlimited coverage.
- [x] Implement the CLI adapter as isolated source modules over the existing database API. Completed 2026-07-09 20:56 EDT.
  CLI mode owns load/search sequencing and bounded path output in `fsearch_cli.c`; the GTK application only dispatches and strips selector flags.
- [ ] Add integration coverage for help, about, limits, environment precedence, and search output.
  Curiosity poke: a missing or stale database must fail clearly and never silently return an empty search.
- [ ] Run the full suite, benchmark suite, inspect the final tree, and commit each green unit.
