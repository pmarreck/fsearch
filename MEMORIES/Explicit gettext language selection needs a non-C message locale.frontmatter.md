---
description: "Explicit gettext language selection needs a non C message locale."
datetime: 2026-07-16T14:10:12-04:00 # America/New_York (EDT)
tags: [explicit, gettext, i18n, l10n, localization, translation, language, selection, non, message, locale]
---
GNU gettext ignores `LANGUAGE=<catalog>` when `LC_MESSAGES` is `C` or
`C.UTF-8`; this is common in reproducible Nix shells. An explicit FSearch
language selection must first set `LANGUAGE` and ensure only `LC_MESSAGES`
uses an available non-C locale, leaving the other libc locale categories
unchanged. The `--lang` release tests deliberately run with `LC_ALL=C.utf8`
to keep that behavior covered. The Linux package wrapper defaults
`LOCALE_ARCHIVE` to `glibcLocales` (without overriding an explicit caller
value), because Nix check sandboxes do not expose the host locale archive.
