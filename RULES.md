# Project Rules

- Keep the GUI behavior unchanged unless GUI mode is explicitly selected.
- Keep CLI code in dedicated modules and communicate with the existing database through supported APIs where possible, minimizing rebase conflicts with upstream UI work.
- Follow test-driven development: write and run a failing deterministic test before each new behavior.
- `./test` runs the complete non-benchmark suite; `./bm` runs optimized benchmarks.
- Do not commit `AGENTS.md` or generated build output.
