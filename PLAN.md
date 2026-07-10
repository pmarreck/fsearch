# Plan

- [x] Establish a pinned Nix build, test, and run environment. Completed 2026-07-09 20:44 EDT.
  Verified the sandboxed flake package runs all existing Meson tests.
- [ ] Add top-level `build`, `test`, and `bm` commands and record benchmark results.
  Curiosity poke: benchmarks must use an optimized build and flag regressions without becoming flaky.
- [ ] Define and test CLI selection, option precedence, and bounded-result rendering.
  Curiosity poke: `--gui` must override `FSEARCH_UI=CLI` without changing existing GUI options.
- [ ] Implement the CLI adapter as isolated source modules over the existing database API.
  Curiosity poke: load/search/result iteration must not leak GTK GUI state or emit malformed path output.
- [ ] Add integration coverage for help, about, limits, environment precedence, and search output.
  Curiosity poke: a missing or stale database must fail clearly and never silently return an empty search.
- [ ] Run the full suite, benchmark suite, inspect the final tree, and commit each green unit.
