# diff16 — Behavioural specification

**Version:** 0.01  
**Status:** implemented; this is the contract the tests check  
**Design:** [DESIGN.md](DESIGN.md) (goals, constraints and open questions)

This document says what `DIFF16.EXE` does. It is written for this project, from DESIGN.md and from the public descriptions of the unified diff format and of the O(ND) difference algorithm; see [PROVENANCE.md](PROVENANCE.md).

## Command line

```text
DIFF16 [-u] [-U N] [-o FILE] [--] OLD NEW
DIFF16 -h
```

| Option | Meaning |
|---|---|
| `-u` | Unified format. It is the default and the only format; the option is accepted for compatibility. |
| `-U N`, `-UN` | `N` unchanged lines of context around each change, `0` to `99`, decimal digits only. Default `3`. |
| `-o FILE`, `-oFILE` | Write the diff to `FILE` instead of standard output (see "Output file"). |
| `-h`, `-?`, `/?`, `--help` | Print help to standard output and exit `0`. |
| `--` | Every later argument is a file name, even one that starts with `-`. |

Options and names may be in any order. An unknown option, a missing or invalid value, or other than two file names is a usage error: a message and a hint on standard error, exit `2`. Options are case-sensitive (`-u` and `-U` differ).

## Exit status

| Status | Meaning |
|---|---|
| `0` | The files have no differences (after line-ending normalization). Nothing is written. |
| `1` | There are differences; the diff was written completely. |
| `2` | Trouble: a usage error, an unreadable or binary input, a write error (including on standard output), no memory, no temporary space, an interruption. Any output may be incomplete; an output file named by `-o` is removed. |

## Lines

Input files are read as bytes, in binary mode; the C runtime does no translation.

A file is a sequence of lines. Each line is its bytes and a terminator:

- **LF** — the line ended with `0Ah`;
- **CR LF** — the line ended with `0Dh 0Ah`;
- **none** — the last line of a file that does not end with a newline.

A CR that is not immediately followed by LF is part of the line, wherever it is, including at the very end of the file. An empty file has no lines. A file ending in a newline has no empty last line.

A NUL byte (`00h`) anywhere in either file is an error ("binary file"), exit `2`. Other bytes, including `1Ah` (Ctrl-Z) and bytes `80h`–`FFh`, are ordinary line content; diff16 does not treat Ctrl-Z as the end of a file.

Lines have no length limit: they are read, compared and written in pieces.

**Comparison.** Two lines are equal when their bytes are equal and either both or neither have a terminator. LF and CR LF are equal: this is a *text-conversion mode* (DESIGN.md §6.1), so that files copied between DOS and Unix compare cleanly. A difference in line endings alone is not a difference, and applying the diff reproduces the content of NEW, not its line endings.

## Output

The diff is a unified diff, written with LF line endings, and nothing at all when there are no differences:

```diff
--- OLD
+++ NEW
@@ -a,b +c,d @@
 context line
-deleted line
+inserted line
```

- **Header.** `--- ` and the OLD name, then `+++ ` and the NEW name, exactly as given on the command line except that each `\` becomes `/`. No timestamps. A name containing a space, a control character (below `20h`) or `7Fh` cannot be written safely in a header; it is an error, exit `2`, before anything is written.
- **Hunks.** Each hunk header is `@@ -a,b +c,d @@`. `b` and `d` are the hunk's line counts in OLD and NEW; `a` and `c` are the 1-based numbers of its first lines. A count of 1 is written without its comma and count (`-a`). For a count of 0, the start is the number of the line *before* the hunk (`-0,0` at the start of a file).
- **Lines.** Each line is a prefix and the line's bytes without its terminator, then LF: a space for context (written from OLD; it is equal to NEW's), `-` for a line deleted from OLD, `+` for a line inserted from NEW. Within a change the deleted lines come before the inserted lines. A CR inside a line, including one at the end of the last line, is kept; the CR of a CR LF terminator is not written.
- **No newline.** After a line with no terminator — the last line of a file without a final newline — the next line of the diff is `\ No newline at end of file`.
- **Context.** Each hunk has up to `N` context lines before its first change and after its last, fewer at the start or end of the file. Two changes separated by at most `2N` equal lines are in the same hunk; changes further apart are in separate hunks. With `-U 0` every change separated by any equal line is its own hunk.
- **Determinism.** The output depends only on the two files and the options: not on free memory, temporary storage, the DOS version or the machine. The host build (`make host`) and `DIFF16.EXE` write byte-identical diffs, which the DOS tests check.

GNU `patch` applies the diff to OLD to produce NEW (with OLD's CR LF terminators normalized, or with `patch --binary` on a normalized copy). diff16 does not write `diff --git`, `index` or mode lines.

## Matching

Which lines are reported as unchanged is diff16's choice, made as follows; any choice gives a correct diff, and the tests check correctness, not that one particular choice is made.

1. **Keys.** Each line gets a 32-bit key from its bytes, its length and whether it has a terminator. Equal lines have equal keys; unequal lines usually differ.
2. **Search.** Keys are compared with the divide-and-conquer form of Myers' O(ND) algorithm: equal lines at the start and end of a region are taken off, then a forward and a reverse search, one edit at a time, meet at a snake that splits the region in two, and so on. It finds a shortest edit script for regions within the limit below.
3. **Limits.** A search stops after `2048` edits each way (`CMP_DMAX`), and the region is split where the forward search reached furthest. Regions are kept on a fixed stack of 64; a region that does not fit becomes a single change. Both bounds keep memory fixed; past them the diff is correct but may be longer than necessary. The bounds are constants, so they do not make the output depend on memory.
4. **Verification.** Lines are then read again, in order, and every pair the search matched is compared byte for byte (with the rule in "Lines"). A pair that differs — two lines with the same key — becomes a change deleting the one and inserting the other. Only verified pairs are written as context. A key collision therefore never produces a wrong diff.

## Memory and temporary files

`DIFF16.EXE` is a compact-model program: code is under 64 KB, and data uses all the conventional memory DOS gives it.

At startup it allocates everything of fixed size: two 4 KB read buffers, the search arrays (two of 4,099 four-byte entries, 32 KB), and two 2 KB page frames for each of four paged arrays (16 KB). It then reads both files once, storing four bytes of key per line in 2 KB pages taken from the heap. When the heap is exhausted, the remaining pages of that array, and of every array after it, are written to a temporary file and read back through the frames. Change lists are paged the same way. Nothing else is allocated later, so running out of memory after startup only moves data to disk.

Lines are never held in memory: diff16 reads each file three times in order (keys, verification, output), and seeks only to rewind.

**Temporary files** are named `D16xxxxx.TMP` (five hex digits from the BIOS timer), created exclusively, so an existing file is never reused or overwritten. They are made in the first directory where creation succeeds among: `%TMP%`, `%TEMP%`, the current directory. All are closed and deleted before diff16 exits, after an error too. A crash, a reboot or a power failure can leave them behind. A full disk while writing one is an error ("disk full"), exit `2`.

Small inputs (the keys of both files and the change lists fit in memory) use no temporary file at all.

**Limits.** A file may have up to 1,073,741,823 lines (`LINE_MAX_COUNT`); more is an error. File size is otherwise limited only by DOS (positions are 32-bit). Line length is unlimited.

## Output file

With `-o FILE`:

- `FILE` must not be either input. diff16 compares the canonical names (INT 21h function 60h, "truename", which resolves `.` and `..`, the current drive and directory, `SUBST`, `JOIN` and `ASSIGN`, and upper-cases and truncates to 8.3; `_fullpath` if that fails) and refuses, exit `2`, if they match or if the check cannot be made. So `OLD.TXT`, `old.txt`, `.\OLD.TXT`, `C:OLD.TXT` and `OLD.TXTX` all name `OLD.TXT`. A network redirector's own aliases are not detected.
- The inputs are opened first, then `FILE` is created or truncated. If diff16 then fails, it removes `FILE`. With no differences `FILE` is left empty.
- Inputs are only ever opened for reading.

Without `-o`, the diff goes to standard output, which is set to binary mode so that the patch's LF line endings are kept when it is redirected to a file. A failing write to standard output (a full disk) is exit `2`, not `1`.

## Diagnostics

Messages go to standard error as `diff16: NAME: WHAT: WHY`. DOS cannot redirect standard error, so they appear on the console.

## Interruption and DOS errors

Ctrl-C and Ctrl-Break (INT 23h) are caught: diff16 stops at its next check, removes its temporary and output files, and exits `2`. DOS checks for Ctrl-C during its own calls, so the response may wait for the next read or write.

Critical errors (INT 24h, such as a drive not ready) are left to DOS, so the user can retry, for example after inserting a disk. Answering Abort ends diff16 without removing its temporary files; Fail makes the call fail, which diff16 reports and cleans up after.

## DOS names

File names are passed to DOS as given; diff16 does not truncate or change them (only the header labels change `\` to `/`). DOS device names are devices whatever their extension, so `NUL.TXT` is the empty device `NUL`, and `CON` reads the keyboard.

## Building and testing

See the README. The tests (`make test`) apply every diff with GNU `patch` and compare the result with NEW, for fixed cases and random pairs, in the release build and in two stress builds: `dtiny` (search limit 4, one page of each array in memory, so the limit and the temporary files are used on small inputs) and `dweak` (most keys collide, so verification matters). The stress builds must produce correct diffs too, only longer ones.
