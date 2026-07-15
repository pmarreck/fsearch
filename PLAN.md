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
- [x] Finish the live Mechatron Prime deployment, verify the public badge endpoint, and add the fork-owned build badge. Completed 2026-07-14 19:21 EDT.
  The exact `master` push `32cc35a8` reached GitHub's FSearch-only webhook with HTTP 200 and the public `badges/fsearch.json` endpoint reported `PASSING` for the declared package, build, and test targets.
- [ ] Replace the upstream translation badge only with an honest fork-owned completeness claim and gate.
  The README now has an interim `10/42` checkpoint badge. Update it to a complete-coverage claim only after the strict gate covers every shipped catalog, including newly added CLI/config strings, fuzzy entries, plural forms, and RTL catalogs.
- [x] Merge the 31 upstream commits added since the fork point without rewriting published history, then rerun `./test`. Completed 2026-07-14 16:32 EDT.
  The three database conflicts preserve upstream scan cancellation/race fixes plus the fork's CLI cancellation and unavailable-root invariants; version assertions now track the upstream 0.3 release.

# Translation Completion (2026-07-14)

- [x] Establish a build-blocking gettext integrity gate for every currently shipped catalog and the CLI/config extraction surface. Completed 2026-07-14 18:07 EDT.
  The default gate verifies syntax and current extraction on every catalog; `--complete` adds the final zero-fuzzy and zero-untranslated requirement so incremental translation checkpoints can remain buildable.
- [x] Extract all human-facing CLI and shared-configuration strings into gettext without translating machine-readable output, query syntax, setting keys, or JSON fields. Completed 2026-07-14 18:07 EDT.
  Curiosity poke: preserve printf/GLib format placeholders exactly across translations.
- [ ] Complete and validate translations for every shipped catalog, then make the translation badge reflect the independently checked result.
  The current checkpoint completes `bg`, `de`, `es`, `eu`, `id`, `it`, `ja`, `nb_NO`, `pt`, `ru`, and `tr`. Curiosity poke: RTL catalogs and new strings must pass the same gate rather than inheriting an optimistic upstream percentage.

# Translation Reliability (2026-07-14)

- [x] Make `translate-po` send bounded PO batches through streamed Responses events and abort only after a genuine idle interval. (2026-07-14 23:03 EDT)
  Curiosity poke: a completed stream must reconstruct bytes exactly and an interrupted stream must never modify the catalog.
- [x] Cover streamed completion, streamed API errors, and a catalog requiring more than one batch before implementing the helper change. (2026-07-14 23:03 EDT)
  Curiosity poke: test the 20-entry boundary and preserve the bearer token's exclusion from process arguments.
- [x] Re-run Basque through the new path, validate its format placeholders, and update the checkpoint only if the whole catalog passes independently. (2026-07-14 23:09 EDT)
  Curiosity poke: `%'d` must remain distinct from `%d` in plural translations.
- [x] Validate printf placeholders even when gettext omitted the `c-format` flag. (2026-07-15 01:25 EDT)
  The independent reviewer found an unflagged multi-`%s` config dump; regression coverage proves it cannot be altered silently.

# Full Catalog Translation (2026-07-15)

- [ ] Complete the six remaining inherited catalogs: French (Canada) (`frc`), Irish (`ga`), Ido (`ie`), Georgian (`ka`), Dari (`prs`), and Urdu (`ur`).
  Run no more than two disjoint catalogs concurrently from canonical `master`, validate and commit small checkpoints, and preserve atomic per-catalog updates. Curiosity poke: rate-limit or stream failure must leave that catalog unchanged and stop the affected checkpoint.
- [ ] Obtain an independent read-only review of each pushed translation checkpoint from `fsearch-i18n`.
  Curiosity poke: the reviewer must validate the committed artifacts rather than trusting the producer's helper or summaries.
- [x] Checkpoint 1: complete French (`fr`) and Dutch (`nl`) through seven streamed batches each. (2026-07-15 01:16 EDT)
  Both catalogs passed strict gettext checks with zero fuzzy and untranslated entries before commit.
- [x] Checkpoint 2: complete Polish (`pl`) and Ukrainian (`uk`) through seven streamed batches each. (2026-07-15 01:30 EDT)
  Both catalogs passed strict gettext checks with zero fuzzy and untranslated entries before commit.
- [x] Checkpoint 3: complete Finnish (`fi`) and Simplified Chinese (`zh_CN`) through seven streamed batches each. (2026-07-15 01:36 EDT)
  Both catalogs passed strict gettext checks with zero fuzzy and untranslated entries before commit.
- [x] Checkpoint 4: complete Hebrew (`he`) and Lithuanian (`lt`), and address reviewer-reported Chinese/Ukrainian keyword omissions. (2026-07-15 01:57 EDT)
  Hebrew retries preserved manually verified `%u` and `%'d` translations when the model omitted their required placeholders; all four catalogs passed strict gettext checks with zero fuzzy and untranslated entries before commit.
- [x] Checkpoint 5: complete English (United Kingdom) (`en_GB`) and Traditional Chinese (`zh_Hant`) through nine streamed batches. (2026-07-15 02:11 EDT)
  Traditional Chinese's `%'d` count and confirmation strings were manually verified before retrying its atomically rejected batch; both catalogs passed strict gettext checks with zero fuzzy and untranslated entries before commit.
- [x] Checkpoint 6: complete Brazilian Portuguese (`pt_BR`) and Korean (`ko`) through nine and eleven streamed batches. (2026-07-15 02:19 EDT)
  Both catalogs completed atomically and passed strict gettext, header/key invariants, the full suite, and the production build before commit.
- [x] Checkpoint 7: complete Romanian (`ro`) and Arabic (`ar`), and distinguish malformed translated PO from placeholder violations. (2026-07-15 02:38 EDT)
  Arabic's six-form `%u` plural and pre-existing `%'d`/`%s` translations were verified before retry; the helper now reports `msgfmt` diagnostics through a red-green regression test.
- [x] Checkpoint 8: complete Swedish (`sv`) and Telugu (`te`), and resolve the reviewer/sweep's unambiguous AppStream keyword findings. (2026-07-15 02:55 EDT)
  Both catalogs completed atomically; Romanian, Portuguese, Brazilian Portuguese, and Indonesian now translate the AppStream `everything` keyword consistently, with strict gettext, header/key invariants, the full suite, and production build passing before commit.
- [x] Preserve established bilingual keyword catalogs and complete Korean's localized keyword set. (2026-07-15 03:00 EDT)
  Japanese, Korean, Russian, and Turkish retain English plus localized AppStream discovery keywords; Korean now includes its existing `모든 것` translation alongside the other Korean terms.
- [x] Restore literal application names and compact AppStream keyword delimiters. (2026-07-15 03:10 EDT)
  Telugu preserves the literal `fsearch` brand token; Swedish and Telugu keyword lists no longer introduce space-prefixed tokens after semicolons.
- [x] Checkpoint 9: complete Hungarian (`hu`), Standard Moroccan Tamazight (`zgh`), and Central Atlas Tamazight (`tzm`). (2026-07-15 04:04 EDT)
  The two Tamazight catalogs deterministically ended a 20-entry preference-string stream without completion; the tested `FSEARCH_TRANSLATE_BATCH_SIZE=10` override re-batched them atomically and both completed with strict gettext, header/key invariants, full suite, and production build passing.
- [x] Checkpoint 10: complete Catalan (`ca`) and Galician (`gl`), and lock invalid batch-size rejection in CI. (2026-07-15 04:16 EDT)
  Both catalogs completed atomically using the proven ten-entry override; the translator regression now verifies invalid batch-size values exit 2 with an actionable error.
- [x] Checkpoint 11: complete Slovak (`sk`) and Marathi (`mr`) using the tested ten-entry batch override. (2026-07-15 08:03 EDT)
  Both catalogs passed strict gettext, template-key and header invariants, with zero fuzzy or untranslated entries before the full-suite gate. Curiosity poke: rich preference strings must not cause a streamed response to end before completion.
- [x] Checkpoint 12: complete Greek (`el`) and Estonian (`et`), and reject omitted multiline PO entries. (2026-07-15 08:15 EDT)
  The translator now counts every PO entry after its required header, including `msgid ""` entries with continuation lines; a red-green regression proves an omitted multiline entry leaves its catalog unchanged. Both catalogs passed corrected dry-run completeness, strict gettext, template-key, header, fuzzy, and untranslated gates. Curiosity poke: completion accounting must inspect PO entry boundaries, not assume a nonempty first `msgid` line.

# 66-Catalog I18n Enforcement (2026-07-15)

- [x] Phase 1: add the 66-locale required-list control file, blessed-hash sentinel, and WARN-only full-catalog completeness check to `./test` and Nix CI. (2026-07-15 10:43 EDT)
  The gate protects the exact list with a committed SHA-256 control value, validates catalog existence, template keys, gettext format correctness, fuzzy entries, and untranslated entries. It currently reports the six inherited catalogs plus 24 not-yet-created catalogs as 30 explicit warnings while exiting successfully. Curiosity poke: the check must enumerate every missing, stale, fuzzy, or untranslated locale without allowing a future edit to silently shrink the required set.
- [ ] Create and complete the 24 missing canonical i18n catalogs with gettext-derived plural headers.
  Curiosity poke: under-resourced and RTL locales require the same structural and semantic review gates as well-resourced ones.
- [ ] Scrub Weblate provenance only from the 17 AI-majority PO headers after all six inherited pending catalogs complete.
  Curiosity poke: compare headers and bodies independently so the 25 Weblate-majority catalogs remain byte-identical.
- [ ] Phase 2: flip the same 66-locale completeness gate from WARN to hard failure after independent verification finds all catalogs complete.
  Curiosity poke: mutation of a catalog or the strictness switch must make CI fail.

# Local Translation Backend (2026-07-15)

- [x] Add a fake-HTTP-tested Ollama backend to `tools/translate-po` while preserving bounded streaming, completed-response checks, exact placeholder validation, and atomic catalog replacement. (2026-07-15 11:10 EDT)
  The native `/api/chat` backend uses a keyless schema-constrained NDJSON stream, rejects a missing `done: true`, defaults to `gemma4:12b` and ten entries per batch, and retains the shared atomic PO merge. A real 20-entry full-catalog batch truncated under the local 4K context limit and left the catalog unchanged; the batch-size override remains available for larger future contexts. Curiosity poke: a local backend must not accidentally require or leak `OPENAI_API_KEY`, and a missing Ollama `done` event must leave the catalog untouched.
- [x] Measure one real 20-entry `gemma4:12b` batch on a disposable catalog before running any complete locale. (2026-07-15 11:10 EDT)
  A warmed French-Canadian batch completed in 25 seconds with zero untranslated entries after PO/gettext/printf validation: approximately 2,880 entries per hour before retry and review overhead. Curiosity poke: record throughput after the model is warm, rather than extrapolating from a single-message prompt.
- [x] Require local-model requests to preserve PO escapes and translate desktop discovery keyword lists. (2026-07-15 11:36 EDT)
  A fake transport proves both explicit instructions travel with each Ollama request. A real ten-entry French-Canadian retry preserved an embedded escaped quote and translated the AppStream list to `recherche;fsearch;fichiers;...;tout;`; malformed PO and an unchanged list had both been atomically rejected first. Curiosity poke: model output is not bitwise deterministic even at temperature zero, so retain the independent syntax and keyword gates.
- [x] Retry only rejected local validation responses, with an atomic three-attempt cap. (2026-07-15 11:47 EDT)
  The fake Ollama transport returns malformed PO then valid PO and proves the current batch alone retries; three malformed responses fail without changing the catalog. The immutable source header is restored before validation, avoiding retries for non-translatable metadata churn. Stream/API failures remain fail-fast. A mutation lowering the cap from three requests to two makes the exhaustion regression fail. Curiosity poke: retries must never turn an unavailable service into an unbounded wait.
- [ ] Translate and independently review the six inherited catalogs, then the 24 gettext-initialized canonical catalogs, using the validated local backend.
  Curiosity poke: under-resourced and RTL locales require structural gates plus later semantic spot-checks rather than a blind mass commit.
