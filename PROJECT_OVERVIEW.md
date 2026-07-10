# FSearch Fork Overview

This fork keeps FSearch's established GTK3 desktop search application and adds a scriptable command-line frontend over the same persisted index and query semantics. The GUI remains the default. The CLI is selected with `--cli` or `FSEARCH_UI=CLI`, unless a later `--gui` selector overrides it.

The initial CLI goal is bounded, machine-friendly path output for a completed query. It must reuse the existing database, parser, matching, sorting, and scan behavior rather than becoming a second search engine.

## Terminology

- **GUI mode**: the existing GTK3 application.
- **CLI mode**: a non-interactive frontend that loads the local FSearch index, evaluates one query, and writes results to standard output.
- **Result limit**: the maximum number of CLI results printed before a visible cap notice is emitted. The default is 100 and can be overridden by `FSEARCH_RESULT_LIMIT`, `--limit`, or `--unlimit`.

## Internationalization phase

**Prepare.** FSearch already uses gettext and has established translation catalogs. New CLI-facing strings will use the same translation mechanism while the CLI surface is still evolving; full locale enforcement follows once the command surface stabilizes.
