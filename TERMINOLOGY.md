# Terminology

- **GUI mode**: the existing GTK3 application.
- **CLI mode**: a non-interactive frontend that loads the local FSearch index, evaluates one query, and writes results to standard output.
- **Result limit**: the maximum number of CLI results printed before a visible cap notice is emitted. The default is 100 and can be overridden by `FSEARCH_RESULT_LIMIT`, `--limit`, or `--unlimit`.
