# Plan

- [ ] Independently review and merge the completed 64-locale translation branch, then change the required-locale gate from WARN-only to blocking.
  Independent review on 2026-07-19 accepted the branch's strict gettext/key/format checks but rejected semantic defects in 22 target-language CLI-help entries (copied Irish example comments) and a Khmer `FSSearch` typo. The correction request is in `../fsearch-i18n/inbox/2026-07-19-independent-review-semantic-blockers.md`; re-review its focused regression before merging. Curiosity poke: structural gettext checks can prove catalog integrity but not low-resource language meaning, so retain a representative maker-independent semantic spot-check and document residual native-review risk.
- [x] Isolate every test execution path from the caller's FSearch configuration and data directories. Completed 2026-07-19 09:09 EDT.
  Red tests first required a private `FSEARCH_TEST_ROOT`; the top-level runner, direct Meson registration, standalone CLI suite, package check phase, and Nix test derivation now set isolated HOME/XDG config/data/cache/state/runtime paths. `./test` and `nix build .#checks.x86_64-linux.test` passed, and Peter's live configuration remained unchanged at its `/home` root with no temporary test path.

- [x] Repair the Nix GTK runtime wrapper so native file choosers can discover their GSettings schema, and lock it with a release-wrapper regression. Completed 2026-07-18 01:56 EDT.
  `wrapGAppsHook3` now packages GTK schemas, GIO modules, and pixbuf data into the release launcher while preserving `LOCALE_ARCHIVE`; the exact Nix check validates the installed wrapper in its isolated sandbox.

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

- [ ] Complete the four remaining required inherited catalogs: Irish (`ga`), Georgian (`ka`), Dari (`prs`), and Urdu (`ur`).
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
- [x] Checkpoint 13: complete French (Canada) (`frc`) through 78 local five-entry batches. (2026-07-15 14:38 EDT)
  The source-owned JSON-slot adapter completed all 388 entries atomically with standard French, zero fuzzy or untranslated entries, matching template keys, gettext-format validation, and a translated AppStream keyword list. Curiosity poke: retain independent semantic review because structural gates cannot establish regional-language quality.
- [x] Checkpoint 14: complete Irish (`ga`) using source-grounded terminology and a bounded independent semantic spot-check. (2026-07-17 13:00 EDT)
  All 388 messages pass gettext syntax, format, and template-key checks with an immutable source header. A representative independent review found no material issue; an external semantic-review request was rate-limited before generating output, so native-speaker polish remains welcome. Curiosity poke: structural gates cannot establish language fluency.

# 64-Catalog I18n Enforcement (2026-07-15)

- [x] Phase 1: add the 64-locale required-list control file, blessed-hash sentinel, and WARN-only full-catalog completeness check to `./test` and Nix CI. (2026-07-15 10:43 EDT)
  The gate protects the exact list with a committed SHA-256 control value, validates catalog existence, template keys, gettext format correctness, fuzzy entries, and untranslated entries. Peter authorized excluding Interlingue (`ie`) and Igbo (`ig`) because neither available translation model clears their semantic-quality floor; the retained Interlingue catalog remains unenforced. It now reports three inherited catalogs plus 23 present-but-untranslated catalogs as 26 explicit warnings while exiting successfully. Curiosity poke: the check must enumerate every missing, stale, fuzzy, or untranslated locale without allowing a future edit to silently shrink the required set.
- [x] Narrow the protected translation-completeness target from 66 to 64 locales. (2026-07-16 09:49 EDT)
  The two excluded locales are covered by a red-green integration assertion that also requires the corresponding 26-locale WARN report. This is an explicit Peter-authorized control-list and checksum update, not an unreviewed list drift.
- [x] Scaffold the 23 missing canonical i18n catalogs with gettext-derived plural headers. Completed 2026-07-17 21:11 EDT.
  All 64 required locales now have a deterministic 388-entry catalog and are listed in `LINGUAS`; the remaining 26 catalogs stay WARN-incomplete until translated. Curiosity poke: under-resourced and RTL locales require the same structural and semantic review gates as well-resourced ones.
- [ ] Scrub Weblate provenance only from the 17 AI-majority PO headers after all four required inherited catalogs complete.
  Curiosity poke: compare headers and bodies independently so the 25 Weblate-majority catalogs remain byte-identical.
- [ ] Phase 2: flip the same 64-locale completeness gate from WARN to hard failure after independent verification finds all catalogs complete.
  Curiosity poke: mutation of a catalog or the strictness switch must make CI fail.

# Locale Selection (2026-07-16)

- [x] Add a shared `--lang CODE` / `FSEARCH_LANG` locale selector before gettext startup, while keeping its option aliases and wider localized-help surface deferred until the catalog rollout is complete. Completed 2026-07-16 14:07 EDT.
  Explicit selection supports `--lang CODE` and `--lang=CODE`, normalizes BCP-47 separators, folds Chinese script/region variants into FSearch's `zh_CN`/`zh_Hant` catalogs, and works from a `C.UTF-8` shell without changing the standard locale path when no override is supplied.
- [x] Cover precedence, normalized locale tags, malformed selectors, `--` handling, and translated CLI/config output with deterministic unit and integration tests. Completed 2026-07-16 14:07 EDT.
  Release coverage verifies `FSEARCH_LANG`, later CLI override precedence, both config argument positions, and GUI stripping before GTK parsing; aliases and broader localized help remain intentionally deferred.
- [x] Make the Nix package provide an available message locale for explicit language selection in hermetic `C.UTF-8` check environments. Completed 2026-07-16 14:18 EDT.
  The Linux package wrapper defaults `LOCALE_ARCHIVE` to `glibcLocales`, preserving any caller-provided archive. The exact Nix `checks.x86_64-linux.test` target and the full suite now prove translated CLI/config output in its isolated sandbox.

# Local Translation Backend (2026-07-15)

- [x] Add a fake-HTTP-tested Ollama backend to `tools/translate-po` while preserving bounded streaming, completed-response checks, exact placeholder validation, and atomic catalog replacement. (2026-07-15 11:10 EDT)
  The native `/api/chat` backend uses a keyless schema-constrained NDJSON stream, rejects a missing `done: true`, defaults to `gemma4:12b` and five entries per batch, and retains the shared atomic PO merge. A real 20-entry full-catalog batch truncated under the local 4K context limit; later ten-entry batches repeatedly omitted content. Both left their source catalog unchanged, and the batch-size override remains available for measured larger contexts. Curiosity poke: a local backend must not accidentally require or leak `OPENAI_API_KEY`, and a missing Ollama `done` event must leave the catalog untouched.
- [x] Measure one real 20-entry `gemma4:12b` batch on a disposable catalog before running any complete locale. (2026-07-15 11:10 EDT)
  A warmed French-Canadian batch completed in 25 seconds with zero untranslated entries after PO/gettext/printf validation: approximately 2,880 entries per hour before retry and review overhead. Curiosity poke: record throughput after the model is warm, rather than extrapolating from a single-message prompt.
- [x] Require local-model requests to produce valid PO and translate desktop discovery keyword lists. (2026-07-15 11:36 EDT)
  A fake transport proves the explicit valid-PO and discovery-keyword instructions travel with each Ollama request. A real ten-entry French-Canadian retry translated the AppStream list to `recherche;fsearch;fichiers;...;tout;`; a real Irish retry recovered from malformed quote handling and passed `msgfmt`. Models may use valid escaped or typographic Unicode quotation marks in translated prose, but must preserve markup and placeholders. Curiosity poke: model output is not bitwise deterministic even at temperature zero, so retain the independent syntax and keyword gates.
- [x] Retry only rejected local validation responses, with an atomic three-attempt cap. (2026-07-15 11:47 EDT)
  The fake Ollama transport returns malformed PO then valid PO and proves the current batch alone retries; three malformed responses fail without changing the catalog. If the model emits a header, the immutable source header is restored by discarding that pre-body block, even when it misspells `msgid` as `msg_id`; a headerless real PO body is preserved rather than losing its first entry. Stream/API failures remain fail-fast. A mutation lowering the cap from three requests to two makes the exhaustion regression fail. Curiosity poke: retries must never turn an unavailable service into an unbounded wait.
- [x] Lower the local Ollama default from ten to five entries after a three-attempt omission exhaustion. (2026-07-15 12:13 EDT)
  The fake eleven-entry catalog changed from `10+1` to `5+5+1` in a red-green regression. A real Irish ten-entry batch omitted requested messages in all three attempts while leaving its source untouched. Curiosity poke: favor a measured smaller default over retrying a deterministic context limit.
- [x] Replace Ollama's PO-generating contract with a JSON translation-slot contract, rendering valid PO only from the source batch locally. (2026-07-15 13:08 EDT)
  Gemma now returns one JSON string per non-header `msgstr` slot; the helper preserves source PO syntax, comments, identifiers, plural declarations, and escaping while inserting those strings locally. Fake red-green tests cover singular slots, plural slots with exact `%'d`, and a too-short response that retries three times atomically. A real five-entry French batch that formerly generated malformed PO now completes on the first attempt with correct quotes and `<code>` markup. Curiosity poke: semantic quality still needs spot-checking because structural validation cannot recognize a fluent-but-wrong translation.
- [x] Require concrete, locale-correct plural headers before translating catalogs with plural entries. (2026-07-15 13:34 EDT)
  The integrity gate rejects absent or placeholder plural rules in every catalog and locks the CLDR-derived French/Persian and singular-at-one formulas for the five previously incomplete source stubs. Curiosity poke: a catalog can pass initial syntax checks while untranslated, then fail only after its first plural translation unless header completeness is checked proactively.
- [x] Preserve source terminal newlines when rendering Ollama translation slots. (2026-07-15 13:54 EDT)
  The local renderer replaces a model response's terminal escaped-newline sequence with the source `msgid` or `msgid_plural` sequence. A red-green fake Ollama test covers a singular slot plus both plural slots whose model responses omit required newlines; a mutation proves it distinguishes an encoded newline from a literal trailing backslash plus `n`.
- [x] Make the root-output gettext key portable across C integer-format macros. (2026-07-15 14:15 EDT)
  The old translatable literal ended in a bare `%` because `xgettext` cannot expand concatenated `G_GINT64_FORMAT`; runtime and catalog keys therefore differed. Format the integer separately, pass it as a sixth `%s`, and preserve the corresponding minimal catalog migration. Curiosity poke: a model's repeated extra placeholder can expose a source-extraction bug rather than a model failure.
- [x] Restore source grouping flags in local decimal placeholders. (2026-07-15 14:24 EDT)
  A fake Ollama response that rewrites `%'d` as `%d` is now corrected only when its source slot uses grouped decimals and no ungrouped decimal placeholder. The exact-placeholder gate still rejects all other differences.
- [x] Preserve translated CLI prose and accelerator labels in local-model requests. (2026-07-15 20:10 EDT)
  Independent review found Gemma had left six CLI diagnostics and Cu_t entirely English in the completed French (Canada) catalog. The fake Ollama request contract now requires explicit examples that distinguish preserved commands/options/metavariables from translatable prose and preserve one accelerator underscore. The ten FRC findings use the independently established standard-French translations. Curiosity poke: nonempty English msgstr values evade gettext completeness, so retain semantic review of local-model checkpoints.
- [x] Send every translation backend a checked target-language name plus locale code. (2026-07-16 09:48 EDT)
  po/LOCALE_NAMES names every protected locale; requests say, for example, Irish (ga) rather than only ga, and the required-locale gate rejects a missing, malformed, duplicate, or drifted mapping. Curiosity poke: language names improve model context but cannot establish semantic quality, so local-model output still needs an independent review.
- [ ] Translate and independently review the four required inherited catalogs, then the 23 gettext-initialized canonical catalogs, using the validated local backend.
  Curiosity poke: under-resourced and RTL locales require structural gates plus later semantic spot-checks rather than a blind mass commit.
