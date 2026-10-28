# diff16

A small text-file comparison tool for 16-bit MS-DOS that writes
**unified diffs**, for `patch16` or any Unix `patch`. Built with Open
Watcom and [libc16](https://github.com/gr-dos/libc16); MIT licensed.

`DIFF16.EXE` is a compact-model, real-mode 8086 executable (about 16 KB)
that runs on DOS 3.0 and later, from an original IBM PC up. It compares
files larger than memory, using temporary files when it must, and never
writes a wrong diff to save memory or time.

    C:\SRC>DIFF16 OLD\MAIN.C NEW\MAIN.C >MAIN.DIF

## Usage

    diff16 [-u] [-U N] [-o FILE] [--] OLD NEW
    diff16 -h

| Option | |
|---|---|
| `-u` | unified format (the default, and the only format) |
| `-U N` | `N` lines of context, 0 to 99 (default 3) |
| `-o FILE` | write the diff to `FILE`, not standard output; never an input file |
| `-h` | help (also `-?`, `/?`) |
| `--` | the next arguments are file names, even if they start with `-` |

Exit status: `0` no differences (nothing is written), `1` differences,
`2` trouble.

## What it compares

- Lines ending in **LF** and in **CR LF** compare equal, so a file copied
  between DOS and Unix is not all changes. The diff is written with LF
  line endings, and applying it gives NEW's text, not its line endings.
  Transfer diffs in binary mode (over a serial link too).
- A last line with **no newline** differs from the same line with one,
  and is marked `\ No newline at end of file`, as `patch` expects.
- A lone CR is part of the line. Ctrl-Z is an ordinary byte.
- A file with a **NUL byte** is binary, and is not compared (status 2).
- Lines may be any length; files may have any number of lines up to
  about a billion.
- File names go into the `---`/`+++` header as typed, with `\` turned
  into `/`. A name with a space or control character can't be written
  safely and is refused.

[docs/SPEC.md](docs/SPEC.md) is the full behaviour, and
[docs/DESIGN.md](docs/DESIGN.md) the design it follows.

## Memory and speed

diff16 keeps four bytes per line in memory, not the lines themselves,
and reads each file three times. About 90 KB of memory is enough to
start; with less than the keys need, the rest go to a temporary file in
`%TMP%`, `%TEMP%` or the current directory, which is removed afterwards.
The diff is the same either way.

Matching uses Myers' O(ND) algorithm, bounded at 2,048 edits per search;
beyond that the diff stays correct but may be longer than the shortest.

Measured under DOSBox-X with an 8086 CPU at a fixed 300 cycles (roughly
4.77 MHz PC speed; an emulator estimate, not real hardware): a 300-line
C file with two changes, 5.5 s; a 9,850-line, 250 KB file with about 200
scattered changes, 3 min 50 s. A 70,000-line pair is compared within
640 KB by spilling to disk.

## Building

On Linux, with `make`, `curl`, `unzip`, `sha256sum` and `xz`:

    make

fetches the pinned libc16 release (`scripts/fetch-libc16`) and the Open
Watcom snapshot it was built with into `build/`, then builds
`diff16.exe`. The link map is checked to contain only diff16's objects
and libc16 (`scripts/check-map`). `make LIBC16_ROOT=../libc16` uses a
libc16 checkout instead.

`make host` builds diff16 for the host with `cc`, for development and
tests; it writes the same diffs as `diff16.exe`.

## Tests

    make test

needs a C compiler, Perl, GNU `patch` and DOSBox-X. The oracle is
`patch`: each diff is checked for well-formed hunks
(`tests/check.pl`), applied to OLD with GNU `patch`, and the result
must be NEW (line endings normalized).

- `make test-host` runs fixed cases (empty files, EOF without newline,
  CR LF, lone CR, hunk boundaries, 70,000-line and 100 KB-line files,
  option and error handling, write errors, temporary-file fallback) and
  300 random pairs (`FUZZ=n` for more), on the release build and two
  stress builds: `diff16-tiny` (search limit 4 and one page of memory per
  array, so the limit and temporary files are always in use) and
  `diff16-weak` (most line keys collide, so byte verification has to
  catch them).
- `make test-dos` runs `diff16.exe` and the stress builds in DOSBox-X:
  `diff16.exe` must write exactly what the host build writes, the
  stress builds' diffs must patch correctly, and `-o` must refuse every
  alias of an input (`.\OLD.TXT`, `C:OLD.TXT`, `OLD.TXTX`, ...).

## Licence

MIT; see `LICENSE`. Source provenance is in
[docs/PROVENANCE.md](docs/PROVENANCE.md).
