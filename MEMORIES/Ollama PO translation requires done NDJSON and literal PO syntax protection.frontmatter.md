---
description: "Ollama PO translation requires done NDJSON and literal PO syntax protection."
datetime: 2026-07-15T11:10:58-04:00 # America/New_York (EDT)
tags: [ollama, llm, ai, language-model, translation, i18n, l10n, localization, ndjson, literal, syntax, protection]
---
The native Ollama `/api/chat` stream is NDJSON rather than OpenAI SSE. A
translation batch must therefore accumulate `message.content` chunks and
require an explicit `done: true` event before parsing its JSON-schema result;
a stream that merely ends is not success.

Gemma 4 returned valid JSON but initially rewrote the required GNU PO keyword
`msgid` as `msg_id`, which `msgfmt` correctly rejected. Keep temperature at
zero and explicitly require immutable PO keywords, while preserving the
existing PO, gettext, exact-printf, completeness, and atomic-merge gates.

With the locally configured 4K context, a real full-catalog 20-entry request
truncated its JSON response and later ten-entry batches repeatedly omitted
entries. Keep the Ollama default at five entries, while allowing an explicit
`FSEARCH_TRANSLATE_BATCH_SIZE` override for models or hardware that have been
measured to sustain a larger batch safely.

Schema-constrained Gemma responses can still remove an escaped quote from PO
text or copy a non-English AppStream semicolon keyword list unchanged. Require
valid PO syntax, invite typographic Unicode quotation marks in translated
prose to avoid fragile ASCII escaping, and explicitly describe the
semicolon-list translation rule (with a short example) in every request. Then
independently reject malformed PO and unchanged non-English lists. Temperature
zero reduces variation but does not make local GPU generation bitwise
deterministic.

The PO header is source-owned metadata, not translated content. When the first
model PO entry is an empty header (including malformed `msg_id`), discard the
model header block and restore the source header before validating the model
body. A response that starts directly with a non-empty `msgid` is headerless,
so preserve it intact rather than losing its first translation. Treat a remaining
PO/placeholder/keyword/completeness validation failure as transient: retry
only that batch, capped at three total attempts, while retaining all atomic
catalog boundaries. Transport, API-error, malformed-stream, and
missing-completion failures are not safe to hide behind retries and must stop
immediately.

Gemma is suitable for translating natural language, not for generating GNU PO
syntax. The Ollama request must therefore ask for a JSON array of plain
translation strings in source-slot order. Render those JSON string literals
into the valid source batch locally, replacing only non-header `msgstr` or
`msgstr[n]` slots. Keep source identifiers, comments, headers, plural fields,
and escaping tool-owned; then run the existing semantic-preservation gates.

Untranslated PO stubs can hide a missing or placeholder `Plural-Forms` header
until their first plural slots are filled. Validate every plural-bearing
catalog proactively, and lock known exceptional locale formulas in the
integrity gate. For the inherited `frc`/`prs` stubs use `n > 1`; for
`ie`/`ka`/`ur` use `n != 1`, derived from CLDR integer cardinal behavior.

GNU gettext also requires each translated string to retain the source's
terminal newline sequence. A local model can omit it while otherwise returning
valid JSON, so derive that escaped suffix from the source `msgid` (or
`msgid_plural` for every plural slot) and replace the response suffix locally.
Count escaping correctly: a literal trailing backslash followed by `n` is not
an encoded newline.

Do not concatenate C printf macros such as `G_GINT64_FORMAT` into a gettext
literal. `xgettext` records only the literal prefix, leaving an invalid bare
percent in the catalog while runtime formatting uses the expanded conversion.
Format the typed value separately and pass it through a normal portable `%s`
placeholder, then migrate that one PO key without a whole-catalog rewrap.

Gemma can remove the apostrophe grouping flag from a `%'d` placeholder. When a
source slot uses grouped decimal formatting and contains no ordinary `%d`,
restore that flag in the rendered data before the exact-placeholder gate. Keep
the correction narrow so every other placeholder mismatch still fails.
