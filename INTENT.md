# FSearch Intent

## Purpose

FSearch is a fast GTK3 desktop file-search application. This fork preserves that application and adds a scriptable command-line interface over the same persisted index, query semantics, and configuration.

## Users And Outcomes

Desktop users can search local files through the existing graphical interface. Shell users can run one completed search and consume matching paths from standard output without maintaining a separate index or query engine.

## Scope

- Preserve the established GTK3 application and its persisted configuration.
- Provide a non-interactive CLI that reuses the database, parser, matching, sorting, and scan behavior.
- Keep GUI and CLI settings, index refresh policy, and localization shared where those concepts apply to both interfaces.
- Keep CLI-specific code in dedicated modules to reduce upstream rebase conflicts.

## Constraints

- GUI mode is the default unless a CLI selector is supplied.
- CLI result output is bounded by default and remains suitable for pipes and scripts.
- `./test` is the complete non-benchmark verification entry point; `./bm` records optimized performance measurements.
- Build and test dependencies are pinned through `flake.nix`.

## Success Evidence

The full test suite passes, the declared flake package and checks build, and the CLI and GUI operate over the same persisted configuration and index.

## Related Documents

- [Terminology](TERMINOLOGY.md)
- [Current plan](PLAN.md)
- [Project rules](RULES.md)
