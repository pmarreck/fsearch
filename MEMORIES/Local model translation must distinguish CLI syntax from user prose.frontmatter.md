---
description: "Local model translation must distinguish CLI syntax from user prose."
datetime: 2026-07-15T20:09:56-04:00 # America/New_York (EDT)
tags: [model, translation, i18n, l10n, localization, distinguish, cli, syntax, user, prose]
---
Gettext structural checks accept a nonempty English msgstr, so a local model
can leave complete CLI usage or diagnostic strings untranslated while every PO,
placeholder, and completeness gate passes. Gemma over-applied the instruction
to preserve CLI options and syntax in frc.po, leaving human prose in six
diagnostics and the Cu_t accelerator label in English.

The Ollama data-only prompt must explicitly preserve only command names,
options, uppercase metavariables, and format placeholders while translating
surrounding prose. It must also say that accelerator labels are translatable
and preserve exactly one underscore before a localized letter. The
fake-transport request test locks concrete Usage: fsearch config get KEY
[--json] and Cu_t -> Cou_per examples into the contract. Continue independent
semantic review; no general structural gate can establish fluent,
locale-appropriate natural-language output.
