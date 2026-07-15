[![🤖 Mechatron Prime](https://img.shields.io/endpoint.svg?url=https%3A%2F%2Fthelio-nixos.tail66c90.ts.net%2Fbadges%2Ffsearch.json)](https://thelio-nixos.tail66c90.ts.net/mechatron-prime/)
[![Localized catalogs](https://img.shields.io/badge/localized%20catalogs-11%2F42-informational?style=for-the-badge)](#localization)

FSearch is a fast file search utility, inspired by Everything Search Engine. It's written in C and based on GTK3.
This fork also provides a non-interactive command-line interface over the same index, query engine, and configuration as
the graphical application.

* For bug reports and feature requests please use the issue tracker: <https://github.com/cboxdoerfer/fsearch/issues>
* For discussions and questions about FSearch use the discussion
  forum: <https://github.com/cboxdoerfer/fsearch/discussions>
* For everything else related to FSearch you can talk to me on Matrix: <https://matrix.to/#/#fsearch:matrix.org>

![](https://raw.githubusercontent.com/cboxdoerfer/fsearch/master/data/screenshots/02-main_window_menubar.png)
![](https://raw.githubusercontent.com/cboxdoerfer/fsearch/master/data/screenshots/01-main_window_headerbar.png)

## Features

- Instant (as you type) results
- [Advanced search syntax](https://github.com/cboxdoerfer/fsearch/wiki/Search-syntax)
- Wildcard support
- RegEx support
- Filter support (only search for files, folders or everything)
- Include and exclude specific folders to be indexed
- Ability to exclude certain files/folders from index using wildcard expressions
- Fast sort by filename, path, size or modification time
- Customizable interface (e.g., switch between traditional UI with menubar and client-side decorations)
- Scriptable CLI search with current-directory, explicit-path, and global scopes
- Shared GUI/CLI configuration commands with machine-readable JSON output
- Extended `glob:` queries with recursive matching, alternatives, ranges, and escaping

## Command-Line Interface

The GUI remains the default when `fsearch` is launched without an interface selector. Use `--cli` for one command, or
set `FSEARCH_UI=CLI` to make CLI mode the default in a shell or profile:

```bash
fsearch --cli --search invoice
FSEARCH_UI=CLI fsearch --search invoice
```

`--gui` explicitly selects the graphical application. Interface selectors are processed from left to right, so the
last `--gui` or `--cli` wins. This makes it possible to override `FSEARCH_UI=CLI` for one invocation:

```bash
fsearch --gui
```

Use the mode-specific help for the complete live command summary. Bare `fsearch --help` describes the graphical
application and points to both CLI help entry points.

```bash
fsearch --cli --help
fsearch config --help
fsearch --cli --about
```

### Search scope

CLI searches print matching absolute paths from the existing FSearch index. Unlike the GUI, CLI search is scoped to
the current directory by default:

```bash
fsearch --cli --search AGENTS
```

Use `--path` to search recursively below another indexed directory, including paths containing spaces, or `--global`
to search every indexed location:

```bash
fsearch --cli --path "$HOME/Documents" --search invoice
fsearch --cli --path "$HOME/Project Files" --search 'ext:pdf'
fsearch --cli --global --search 'ext:jpg;png'
```

`--path` and `--global` may appear in any option order. When conflicting scope options are repeated, the later option
wins.

### Query syntax

CLI mode uses the same query parser and saved search settings as the GUI. A space is an implicit `AND`; `AND` or `&&`
makes it explicit. `OR` or `||` selects alternatives, `NOT` or `!` negates a term, and parentheses group expressions.
Quote the entire query for the shell whenever it contains spaces or shell metacharacters.

```bash
fsearch --cli --global --search 'report AND ext:pdf'
fsearch --cli --global --search '(draft OR final) !archive'
```

Common query modifiers are:

| Modifier | Meaning | Example |
|----------|---------|---------|
| `file:` | Match files only; with `glob:`, match the filename instead of the full path | `file:size:>20mb` |
| `folder:` | Match folders | `folder:Projects` |
| `path:` | Match against the complete path | `path:"Project Files"` |
| `ext:` | Match one or more semicolon-separated extensions | `ext:jpg;png` |
| `size:` | Compare indexed file size | `size:>20mb` |
| `dm:` | Match indexed modification dates | `dm:today` |
| `case:` | Force case-sensitive matching for the following term | `case:AGENTS` |
| `regex:` | Interpret the following term as a PCRE2 regular expression | `file:regex:".+\.pdf$"` |
| `glob:` | Interpret the following term as the extended glob syntax below | `glob:**/*.png` |

Modifiers compose. For example, this matches either conventional agent-instruction filename at any depth,
case-sensitively:

```bash
fsearch --cli --global --search 'case:glob:"**/{AGENTS,CLAUDE}.md"'
```

Regular-expression matching follows the shared case setting. Prefix the expression with `case:` when it must be
case-sensitive regardless of that setting:

```bash
fsearch --cli --global --search 'case:regex:"(AGENTS|CLAUDE)\.md$"'
```

#### Extended glob syntax

`glob:` searches complete paths by default. Add `file:` when the pattern should inspect only the filename. This is
important because `*` does not cross a directory separator, while `**` does.

| Syntax | Meaning |
|--------|---------|
| `*` | Zero or more characters except `/` |
| `**` | Zero or more characters including `/` |
| `**/` | Zero or more directory levels |
| `?` | Exactly one non-`/` character |
| `[a-c]` | One character in a range |
| `[abc]` | One character from a set |
| `{png,jpg}` | One of the listed alternatives |
| `{01..12}` | An inclusive numeric range |
| `\*` | A literal `*`; escaping also protects other glob metacharacters |

Examples:

```bash
fsearch --cli --global --search 'glob:**/*.{png,jpg}'
fsearch --cli --global --search 'glob:**/report-{01..12}.pdf'
fsearch --cli --global --search 'file:glob:README.??'
fsearch --cli --global --search 'case:glob:"**/Project Files/**/[A-C]*.md"'
```

### Result limits and output

CLI mode prints one matching path per line. The default limit is 100 results, which prevents an unexpectedly broad
query from flooding the terminal. Configure it in any of three ways:

```bash
fsearch --cli --global --search report --limit 250
FSEARCH_RESULT_LIMIT=250 fsearch --cli --global --search report
fsearch --cli --global --search report --unlimit
fsearch --cli --global --search report --unlimited
```

`--limit` overrides `FSEARCH_RESULT_LIMIT`. `--unlimit` and `--unlimited` are exact synonyms and disable the cap. As
with other conflicting options, the later CLI option wins, so `--unlimit --limit 20` is capped at 20.

When a query exceeds its limit, FSearch prints the bounded paths followed by a styled cap notice. Otherwise, it prints
the total match count to stderr. For a path-only stream, disable the cap and redirect stderr independently:

```bash
fsearch --cli --global --search 'ext:pdf' --unlimit 2>/dev/null
```

### Index loading, updates, and interruption

CLI mode reuses the GUI's persisted index; it does not walk the filesystem for every query. If the index is missing or
its configured roots or exclusions have changed, the CLI rebuilds it before completing the search. The update notice
goes to stderr. On an interactive terminal, an indeterminate status line reports the observed entry count, scan rate,
and current path without inventing a completion percentage.

To rescan the configured index without searching:

```bash
fsearch --cli --update-database
fsearch --cli -u
```

On Unix, `Ctrl-C` cancels an active load, scan, or search and exits with status 130.

### Shared GUI and CLI configuration

`fsearch config` views and changes the same persisted configuration used by both interfaces. It does not require
`--cli`. Search settings apply to CLI queries as well as GUI queries, and roots, exclusions, and saved filters remain
shared. Close the GUI before mutating configuration; the CLI detects a running GUI and refuses the change rather than
risk having stale in-memory GUI state overwrite it.

General configuration commands:

| Command | Purpose |
|---------|---------|
| `fsearch config path` | Print the active configuration file path |
| `fsearch config show [SECTION] [--json]` | Show all scalar settings or one section |
| `fsearch config get KEY [--json]` | Print one typed scalar setting |
| `fsearch config set KEY VALUE` | Set a boolean, integer, string, or enumerated setting |
| `fsearch config unset KEY` | Restore one scalar setting to its built-in default |
| `fsearch config reset [KEY\|SECTION] --yes [--rescan]` | Reset one key, one section, or everything |
| `fsearch config doctor` | Check whether configuration and index state agree |

Run `fsearch config show` to discover every scalar key and its current value. The available sections are `search`,
`application`, `window`, `ui`, `columns`, `sort`, and `dialogs`.

```bash
fsearch config show search
fsearch config show search --json
fsearch config get search.match-case
fsearch config set search.match-case true
fsearch config unset search.match-case
fsearch config reset search --yes
fsearch config doctor
```

#### Indexed roots

```bash
fsearch config roots list
fsearch config roots list --json
fsearch config roots add "$HOME/Code" --monitor --one-file-system --rescan
fsearch config roots set "$HOME/Code" --scan-after-launch --rescan-every 3600
fsearch config roots remove "$HOME/Code" --rescan
```

`roots add` and `roots set` accept `--active`/`--inactive`, `--monitor`/`--no-monitor`,
`--one-file-system`/`--no-one-file-system`, `--scan-after-launch`/`--no-scan-after-launch`, and
`--rescan-every SECONDS`; zero disables periodic rescanning. Add `--rescan` to rebuild immediately.

#### Index exclusions

```bash
fsearch config excludes list
fsearch config excludes list --json
fsearch config excludes add '*.tmp' --type wildcard --scope basename --target files
fsearch config excludes add '/private/.*' --type regex --scope full-path --target both --rescan
fsearch config excludes hidden true --rescan
fsearch config excludes remove '*.tmp' --type wildcard --scope basename --target files
```

Exclusions may be `fixed`, `wildcard`, or `regex`; may inspect `full-path` or `basename`; and may target `both`, `files`,
or `folders`. New exclusions can be `--active` or `--inactive`. The same selectors identify the rule removed by
`excludes remove`.

#### Saved filters

```bash
fsearch config filters list
fsearch config filters list --json
fsearch config filters add 'Large PDFs' --query 'ext:pdf size:>20mb' --macro largepdfs
fsearch config filters set 'Large PDFs' --query 'ext:pdf size:>100mb' --case
fsearch config filters remove 'Large PDFs'
fsearch config filters reset
```

`filters add` and `filters set` accept `--query QUERY`, `--macro MACRO`, `--case`/`--no-case`,
`--path`/`--no-path`, and `--regex`/`--no-regex`. Paired switches and other conflicting options use the later value.

Root and exclusion changes mark the existing index stale. The next CLI search rebuilds it automatically, or `--rescan`
can apply the change immediately. `show`, `get`, and collection `list` commands support `--json` for scripts. Use the
nested help pages for the canonical syntax:

```bash
fsearch config roots --help
fsearch config excludes --help
fsearch config filters --help
```

## Requirements

- GTK 3.22
- GLib 2.60
- glibc 2.19 or musl 1.1.15 (other C standard libraries might work too, those are just the ones I verified)
- PCRE2 (libpcre2) 10.21
- ICU 4.4

## Download

It is recommended to install FSearch from one of the **Stable** packages, unless you know what you're doing.

The **Development** packages are primarily intended for testing and adventurous users.


| Distribution                                                                                          | Stable                                                                                                                     | Development                                                                            |
|-------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------|
| Ubuntu                                                                                                | [PPA Stable](https://launchpad.net/~christian-boxdoerfer/+archive/ubuntu/fsearch-stable)                                   | [PPA Daily](https://launchpad.net/~christian-boxdoerfer/+archive/ubuntu/fsearch-daily) |
| Arch Linux                                                                                            | [AUR](https://aur.archlinux.org/packages/fsearch/)                                                                         | [AUR (git)](https://aur.archlinux.org/packages/fsearch-git/)                           |
| Fedora/RHEL/CentOS                                                                                    | [COPR Stable](https://copr.fedorainfracloud.org/coprs/cboxdoerfer/fsearch/)                                                | [COPR Nightly](https://copr.fedorainfracloud.org/coprs/cboxdoerfer/fsearch_nightly/)   |
| Debian                                                                                                | [OpenBuildService](https://software.opensuse.org//download.html?project=home%3Acboxdoerfer&package=fsearch#manualDebian)   |                                                                                        |
| openSUSE                                                                                              | [OpenBuildService](https://software.opensuse.org//download.html?project=home%3Acboxdoerfer&package=fsearch#manualopenSUSE) |                                                                                        |
| Flatpak ([limited features](https://github.com/cboxdoerfer/fsearch/wiki/Flatpak-version-limitations)) | [Flathub](https://flathub.org/apps/details/io.github.cboxdoerfer.FSearch)                                                  |                                                                                        |
| Solus*                                                                                             | [Solus Repository](https://dev.getsol.us/source/fsearch/)                                                                  |                                                                                        |
| FreeBSD*                                                                                           | [FreshPorts](https://www.freshports.org/sysutils/fsearch)                                                                  |                                                                                        |

(*) Not maintained by me

## Roadmap

<https://github.com/cboxdoerfer/fsearch/wiki/Roadmap>

## Build Instructions

<https://github.com/cboxdoerfer/fsearch/wiki/Build-instructions>

## Nix Development

This fork provides a pinned Nix environment:

- `nix build` builds the optimized package.
- `nix run . -- --cli --help` runs the CLI frontend without installing it.
- `nix run . -- --cli --global --search 'ext:pdf'` runs an indexed search.
- `nix run . -- config --help` shows the shared configuration CLI.
- `nix flake check` builds and runs unit plus CLI integration checks.
- `./build`, `./test`, and `./bm` provide the corresponding project commands. `./bm` records and compares the optimized CLI startup baseline.

## Localization

This fork localizes FSearch with AI-assisted machine translation, applied directly to the shipped catalogs via `tools/translate-po` and gated by strict PO, gettext, and exact printf-placeholder validation before every commit.

> Upstream project uses a slow, human-gated Weblate translation service that has taken years to add just a handful of translations when AI does high quality translation in a single afternoon. One of these things is the correct thing to be offended by.

The current CLI/config localization checkpoint is complete for Basque (`eu`), Bulgarian (`bg`), German (`de`),
Spanish (`es`), Indonesian (`id`), Italian (`it`), Japanese (`ja`), Norwegian Bokmal (`nb_NO`), Portuguese (`pt`),
Russian (`ru`), and Turkish (`tr`). The remaining shipped catalogs are still in progress; do not interpret this list
as a complete translation-coverage claim.

For maintainers, `./test` verifies every shipped catalog parses correctly, is current with the extracted template, and
includes the human-facing CLI and configuration text. The stricter command below also requires every catalog to have no
fuzzy or untranslated strings; it is the required gate before claiming complete translation coverage:

```bash
nix develop -c bash tests/i18n/test_catalogs . --complete
```

`tools/translate-po --dry-run po/LANGUAGE.po` reports the outstanding strings in one catalog without changing it.
`tools/translate-po --apply po/LANGUAGE.po` sends at most 20 entries per streamed request and replaces the catalog
only after every batch has completed and passed PO, gettext, and exact printf-placeholder validation.

## Current Limitations

* Sorting lots of results by *Type* can be very slow, since gathering that information is expensive, and the data isn't
  indexed. This also means that when the view is sorted by *Type*, searching will reset the sort order to *Name*.
* Using the *Move to Trash* option doesn't update the database index, so trashed files/folders show up in the result
  list as if nothing happened to them.

## Why yet another search utility?

Performance. On Windows I really like to use Everything Search Engine. It provides instant results as you type for all
your files and lots of useful features (regex, filters, bookmarks, ...). On Linux I couldn't find anything that's even
remotely as fast and powerful.

Before I started working on FSearch, I took a look at existing solutions. I tried MATE Search Tool (formerly GNOME
Search Tool), Recoll, Krusader (locate based search), SpaceFM File Search, Nautilus, ANGRYsearch and Catfish, to find
out whether it makes sense to improve those. However, they're not exactly what I was looking for:

- standalone application (not part of a file manager)
- written in a language with C like performance
- no dependencies to any specific desktop environment
- Qt5 or GTK3 based
- small memory usage (both hard drive and RAM)
- target audience: advanced users

## Why GTK3 and not Qt5?

I like both of them, and my long term goal is to provide console, GTK3 and Qt5 interfaces, or at least make it easy for
others to build those. However, for the time being it's only GTK3 because I like C more than C++, and I'm more familiar
with GTK development.

## Questions?

Email: christian.boxdoerfer[AT]posteo.de
