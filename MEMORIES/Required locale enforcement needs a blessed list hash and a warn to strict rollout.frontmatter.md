---
description: "Required locale enforcement needs a blessed list hash and a warn to strict rollout."
datetime: 2026-07-15T10:43:27-04:00 # America/New_York (EDT)
tags: [required, locale, i18n, l10n, localization, translation, enforcement, blessed, list, hash, warn, strict, rollout]
---
Locale-completeness checks are only meaningful when their required locale set is
itself protected. `po/REQUIRED_LOCALES` therefore has a committed SHA-256
sentinel that the checker validates before catalog checks begin, making any
scope change a conspicuous two-file review event.

For a catalog rollout, run the same exhaustive checker in `--warn` mode while
translations are incomplete, then switch its invocation to `--strict` only
after every required locale passes. This preserves visible CI evidence without
creating a deliberately red build window.
