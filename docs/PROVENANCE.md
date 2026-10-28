# diff16 — Source provenance ledger

DESIGN.md §2.2 asks for a ledger of where each part of diff16 came from. diff16 is MIT licensed (`LICENSE`).

## How the code was written

The first implementation (version 0.01, 2026-10-28) was written in one session by Claude (an Anthropic AI model, via Claude Code), working for Graham Ollis from DESIGN.md. No source code of any diff program was opened, consulted or copied during the work: not GNU diffutils, BSD diff, BusyBox, or any DOS diff. The algorithm was written from its published description (below). Existing programs were used only as black boxes in the tests, on inputs made for the tests.

An AI model's general knowledge comes from training on a large body of text, which includes public source code; the model was not shown, and did not reproduce, any particular implementation. Reviewers who want more assurance can use similarity checks, as DESIGN.md §2.2 suggests, as supporting evidence.

## Modules

| File | What it does | Written from |
|---|---|---|
| `src/d16.h` | shared types and declarations | original |
| `src/main.c` | options, labels, `-o` checks, the order of the passes | DESIGN.md §4–5 |
| `src/lines.c` | reading lines in segments, CR LF handling, line keys, byte comparison | DESIGN.md §6; the key is an original pair of 16-bit hashes (a 5-bit rotate-xor and a multiply-by-31 sum) chosen for speed on an 8088 |
| `src/bigarr.c` | paged arrays that spill to a temporary file | original, DESIGN.md §8 |
| `src/compare.c` | matching: Myers' O(ND) algorithm, divide and conquer, with an edit limit and a fixed stack | E. W. Myers, "An O(ND) Difference Algorithm and Its Variations", *Algorithmica* 1 (1986), pp. 251–266: the greedy forward search, the reverse search and middle snake, and the linear-space divide and conquer. The diagonal bounds checks, the limit heuristic and the explicit stack are this project's |
| `src/verify.c` | byte-for-byte verification of every matched pair | original, DESIGN.md §7.3 |
| `src/unified.c` | hunks and the unified diff | the unified format as POSIX.1-2008 `diff -u` describes it, and the GNU diffutils *manual* for `\ No newline at end of file` and the empty-range form of hunk headers; checked against GNU `patch` |
| `src/output.c` | buffered output, decimal numbers, no `*printf` | original |
| `src/temp.c` | temporary file names, creation and removal | original, DESIGN.md §8 |
| `src/error.c` | messages and the trouble exit | original |
| `src/sys.c` | DOS and POSIX host calls | libc16's specs (`docs/spec/`), Ralf Brown's Interrupt List for INT 21h functions 41h and 60h and INT 23h |
| `tests/host.sh`, `tests/dos.sh`, `tests/gen.pl`, `tests/check.pl` | tests | original; `dos.sh` follows the DOSBox-X approach of the other gr-dos tools' tests (same author) |
| `scripts/fetch-libc16` | fetch the pinned libc16 release | copy of libc16's `consumers/fetch-libc16` (MIT), with this project's version and SHA-256 |
| `scripts/check-map` | check that only diff16 and libc16 are linked | original |
| `Makefile` | build | follows the gr-dos utilities' Makefiles (hello16, tico16; same author, MIT) |

## Runtime

`DIFF16.EXE` links only its own objects and libc16 0.04 (`LIBC16C.LIB`, MIT), pinned by version and SHA-256 in `scripts/fetch-libc16`. `scripts/check-map` fails the build if the link map shows any other module. diff16 adds nothing to libc16; the two DOS calls libc16 does not have (delete a file, canonical name) are in `src/sys.c` through libc16's `intdosx`.

## Test oracles

Used as black boxes only:

- **GNU patch 2.8** applies every diff the tests make; its result must equal NEW. This is the primary oracle (DESIGN.md §10.2).
- **GNU diff** was run by hand on a few inputs while writing, to compare formats; the tests do not use it.
- **DOSBox-X** runs `DIFF16.EXE`.
