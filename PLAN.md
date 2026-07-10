# Plan

- [x] Establish a pinned Nix build, test, and run environment. Completed 2026-07-09 20:44 EDT.
  Verified the sandboxed flake package runs all existing Meson tests.
- [x] Add top-level `build`, `test`, and `bm` commands and record benchmark results. Completed 2026-07-09 21:01 EDT.
  The benchmark uses Release Nix output, records a baseline, and requires a rerun to accept a change larger than 20%.
- [x] Define and test CLI selection, option precedence, and bounded-result rendering. Completed 2026-07-09 20:49 EDT.
  The pure selector gives later `--gui`/`--cli` flags precedence over `FSEARCH_UI`; limit resolution has default, environment, argument, and unlimited coverage.
- [x] Implement the CLI adapter as isolated source modules over the existing database API. Completed 2026-07-09 20:56 EDT.
  CLI mode owns load/search sequencing and bounded path output in `fsearch_cli.c`; the GTK application only dispatches and strips selector flags.
- [x] Add integration coverage for help, about, and environment precedence. Completed 2026-07-09 21:01 EDT.
  Remaining query-result fixture coverage belongs with a future database-fixture harness rather than real user index data.
- [x] Make a failed configured index root leave the database searchable and saveable. Completed 2026-07-09 21:39 EDT.
  Regression coverage builds one available root plus one unavailable root, verifies the available match remains searchable, and disposes the database to exercise the save path.
- [x] Make CLI searches rebuild an unavailable index before querying it. Completed 2026-07-09 22:28 EDT.
  An isolated CLI integration test starts with a missing index, requires the italic stderr status line, waits for rebuild completion, and verifies the expected path result.
- [x] Add live CLI index status, result summaries, and query-syntax discoverability. Completed 2026-07-10 08:10 EDT.
  Indexing reports observed entry count, rate, and current path without inventing a percentage; uncapped searches report their total to stderr; help covers both unlimited aliases, `case:`, and case-sensitive regex.
- [x] Bound parallel regex workers and support CLI interruption. Completed 2026-07-10 08:19 EDT.
  Regex match data has 32 per-thread slots, so index-store workers are capped to that invariant; regressions cover a 1,024-entry parallel regex search and a `SIGINT` exit with status 130.
- [ ] Run the full suite, benchmark suite, inspect the final tree, and commit each green unit.
