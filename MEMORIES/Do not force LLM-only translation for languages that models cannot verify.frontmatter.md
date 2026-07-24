---
description: "Do not force LLM only translation for languages that models cannot verify."
datetime: 2026-07-16T09:50:15-04:00 # America/New_York (EDT)
tags: [force, llm, ai, language-model, translation, i18n, l10n, localization, languages, models, verify]
---
Gemma and Qwen fabricated plausible but false Irish, and Claude independently
identified the same low-confidence failure mode for Interlingue and Igbo.
Gettext syntax, exact placeholders, and non-English keyword checks establish
structure but cannot certify meaning. Two agreeing general-purpose LLMs are
not an independent semantic oracle for a language outside both models' reliable
coverage.

Peter authorized removing `ie` and `ig` from the protected locale-completeness
set rather than shipping fabricated translation. Keep existing `po/ie.po` as
upstream history but unenforced. Before restoring either locale, require an
authoritative source, native review, or a separately evaluated
translation-specialized model; then make the control-list/checksum change and
restore the exact-map entry together.
