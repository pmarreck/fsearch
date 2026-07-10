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
- [x] Run the full suite, benchmark suite, inspect the final tree, and commit each green unit. Completed 2026-07-10 08:45 EDT.
# CLI and glob follow-up (2026-07-10)

- [x] Add a first-class `glob:` query modifier backed by an enhanced PCRE2 conversion. Completed 2026-07-10 08:45 EDT.
  `*`, `**/`, `?`, character ranges, brace alternatives, bounded numeric ranges, and escapes are covered as query classifiers.
- [x] Make `./build`, `./test`, and `./bm` fulfill the documented build, aggregate-test, and performance-log contracts. Completed 2026-07-10 08:45 EDT.
  The benchmark history records UTC timestamp, commit, wall time, and CPU time.

# Benchmark scaling follow-up (2026-07-10)

- [x] Record a CPU-independent O(n) scaling exponent from 1x, 2x, 4x, and 8x glob workloads. Completed 2026-07-10 08:57 EDT.
  Raw wall, user, and system timings include OS/version, CPU model/MHz/cores, memory, and architecture; the independent log-log slope is the cross-machine comparison field.

# Static analysis follow-up (2026-07-10)

- [x] Add modeled Cppcheck analysis as a blocking release-build check. Completed 2026-07-10 09:19 EDT.
  `tests/static-analysis` uses Cppcheck's GTK/GLib model and exact inline suppressions only for intentional token/index pointer storage; Meson compilation independently catches the model's external-header parse noise.

- [ ] Profile and reduce `./test` wall time without weakening its independent unit, CLI, package, or static-analysis controls.
  Curiosity poke: avoid paying separately for local Meson compilation, release-package compilation, and whole-project analysis when a shared build database or a Nix check can supply the same evidence.

# Shared CLI configuration (2026-07-10)

- [x] Make CLI search derive the same query flags as the GUI from the shared configuration. Completed 2026-07-10 10:21 EDT.
  `config_get_search_query_flags()` is the shared resolver; an end-to-end CLI regression verifies case-sensitive GUI configuration affects CLI search.
- [x] Add a GTK-free configuration command API for scalar settings and database roots/exclusions. Completed 2026-07-10 11:35 EDT.
  Curiosity poke: write only fully validated configuration and make index-invalidating mutations explicit.
- [x] Add CLI configuration commands for saved filters, help, JSON output, and integration coverage. Completed 2026-07-10 11:35 EDT.
  Curiosity poke: collection selectors must remain stable and non-destructive across GUI edits.
  The isolated adapter covers every persisted scalar plus roots, exclusions, hidden-file policy, and saved filters; option ordering, invalid input, JSON, GUI concurrency, 64-bit persistence, stale-index warnings, immediate rescans, and first-run data directories have regressions. Main help points to the detailed config help, and the man page documents every command family.
- [x] Make GUI-default help disclose the CLI and configuration help entry points. Completed 2026-07-10 13:44 EDT.
  Bare `fsearch --help` retains GTK options while pointing to `fsearch --cli --help` and `fsearch config --help`; release CLI coverage prevents the discoverability regression.
