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

# CLI README documentation (2026-07-14)

- [x] Document the complete CLI search, query, output-limit, index, and shared-configuration workflows in `README.md`. Completed 2026-07-14 15:57 EDT.
  Curiosity poke: keep examples executable and distinguish path output on stdout from status and diagnostic output.
- [x] Remove the obsolete recommendation to use other command-line search tools, validate the documented surface against release help, and run the documentation-relevant tests. Completed 2026-07-14 15:57 EDT.
  Curiosity poke: nested configuration help remains the canonical detail source, so the README must be thorough without silently inventing aliases.
- [ ] Finish the live Mechatron Prime deployment, verify the public badge endpoint, and add the fork-owned build badge.
  Source policy `46db1db` and NixOS vendor commit `27f431a` register the audited package/build/test targets; activation and exact webhook provisioning remain. Curiosity poke: a README badge is not CI evidence until the exact-name webhook, worker policy, live build, and public JSON route all agree.
- [ ] Replace the upstream translation badge only with an honest fork-owned completeness claim and gate.
  The inaccurate upstream badge was removed; the current 42 upstream catalogs still contain 6,001 fuzzy or untranslated entries, before extracting CLI/config strings. Curiosity poke: 100% must include newly added CLI/config strings, fuzzy entries, plural forms, and RTL catalogs rather than measuring only the upstream GUI template.
- [x] Merge the 31 upstream commits added since the fork point without rewriting published history, then rerun `./test`. Completed 2026-07-14 16:32 EDT.
  The three database conflicts preserve upstream scan cancellation/race fixes plus the fork's CLI cancellation and unavailable-root invariants; version assertions now track the upstream 0.3 release.

# Translation Completion (2026-07-14)

- [x] Establish a build-blocking gettext integrity gate for every currently shipped catalog and the CLI/config extraction surface. Completed 2026-07-14 18:07 EDT.
  The default gate verifies syntax and current extraction on every catalog; `--complete` adds the final zero-fuzzy and zero-untranslated requirement so incremental translation checkpoints can remain buildable.
- [x] Extract all human-facing CLI and shared-configuration strings into gettext without translating machine-readable output, query syntax, setting keys, or JSON fields. Completed 2026-07-14 18:07 EDT.
  Curiosity poke: preserve printf/GLib format placeholders exactly across translations.
- [ ] Complete and validate translations for every shipped catalog, then make the translation badge reflect the independently checked result.
  First checkpoint completes `bg`, `de`, `es`, `id`, `it`, `ja`, `nb_NO`, `pt`, `ru`, and `tr`; Basque requires a separately approved retry after an API-side generation stall. Curiosity poke: RTL catalogs and new strings must pass the same gate rather than inheriting an optimistic upstream percentage.
