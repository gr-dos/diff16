# diff16 — Exploration and Design

**Status:** Proposed / exploration  
**Date:** 2026-10-28  
**Proposed repository:** `gr-dos/diff16`  
**Intended license:** MIT  
**Target:** 8088/8086-compatible MS-DOS 3.0+ and GR-DOS; Open Watcom C; `libc16`

## 1. Executive summary

`diff16` is a **new, independently implemented**, small, native 16-bit DOS text-file comparison tool. Its primary job is to produce portable **unified diffs** suitable for consumption by `patch16` or modern Unix patch tools. It should also provide a useful human-readable comparison on a real IBM PC/XT-class machine. It will not port GNU diffutils or derive code from any preexisting diff implementation.

The implementation must remain useful within conventional DOS memory, avoid unnecessary dependence on `*printf`-family functions, and use the MIT-licensed `libc16` runtime rather than the Open Watcom proprietary runtime. It should be able to compare files larger than available memory without corrupting data or silently producing incorrect output.

The project prioritizes **correct and deterministic patches, safe failure, predictable memory use, and interoperability** over minimum edit-script size or GNU feature parity.

## 2. Project boundaries and licensing

### 2.1 License policy by repository

| Repository / component | Permitted source provenance | Rule for missing functionality |
|---|---|---|
| `gr-dos/diff16` | **MIT**; new original code written for this project | Implement from public specifications and independent reasoning; default new code to **MIT**. Do **not** copy GNU/GPL, historical noncommercial, or proprietary diff implementation code into `diff16`. |
| `gr-dos/libc16` | **MIT only** | Reuse existing MIT functions; otherwise develop a **clean-room MIT** implementation, with its own specification, tests, and provenance. |
| `gr-dos/patch16` | Separate project, potentially using Wall-derived/GPL material under its own verified licenses | `diff16` communicates with it through the documented unified-diff *format*, not linked source or shared GPL code. |

**Rule:** GPL or Larry Wall-licensed `patch16` code must not enter `libc16`, nor should it be copied into the MIT-only `diff16`. A format shared between independently implemented programs does not imply shared implementation code. The `patch16` license remains its own design/legal decision.

### 2.2 Clean-room development requirements

- Write the implementation from an original, project-specific behavioral specification and publicly documented file-format descriptions. An algorithm described in academic literature may guide design, but do not transcribe a particular implementation.
- Avoid looking at GNU diffutils, BSD `diff`, historical proprietary DOS diff implementations, or other source to reproduce their code. Existing programs may be used as **black-box test oracles**, with independently created test inputs and captured expected behavior.
- Keep a provenance ledger for source modules, specifications, authorship, and independently developed algorithms. Any external code must have verified MIT permission **before** being imported; license compatibility alone does not authorize copying.
- Implement new `libc16` functionality separately in its repository: specify API and DOS semantics, create unit/ABI tests in small/compact/large models, verify no proprietary runtime linkage, and release/version it before pinning from `diff16`.
- Existing `libc16` public APIs should be preferred, even when a specialized helper might seem convenient. Prefer `diff16`-local code for functionality used only by `diff16`.
- Use source provenance and similarity checks as supporting evidence, not as substitutes for licensing review or an independent implementation process.

### 2.3 Distribution

Deliver `DIFF16.EXE`, source, build instructions, a copy of the MIT license, and documentation. `libc16` remains independently MIT licensed. No Open Watcom runtime objects or libraries may appear in a release executable. The binary must run without GR-DOS-specific kernel services beyond standard DOS interfaces.

## 3. Goals and non-goals

### Goals

1. Produce a correct unified diff between two DOS text files using `diff16 -u OLD NEW`.
2. Support `-U N` for the number of context lines, with a sensible default of three.
3. Return conventional exit statuses: `0` identical, `1` differences found, `2` usage/I/O/resource error.
4. Read files with LF, CRLF, and a final line lacking a terminator; define line-ending treatment precisely.
5. Handle inputs larger than free conventional memory using bounded RAM and temporary storage when necessary.
6. Emit deterministic output, suitable for transfer to Linux or use with `patch16` on DOS.
7. Be buildable and testable on Linux using pinned Open Watcom/JWasm and `libc16`, and runnable on an original 8088.
8. Avoid `printf`, `fprintf`, `sprintf`, `snprintf`, `scanf` and similar formatting/parsing families; prefer bounded routines writing strings, decimal integers, and fixed syntax directly.

### Explicit non-goals for v0.01

- Recursive directory comparison, Git repository semantics, file modes, symlinks, binary patch format, timestamps that imply metadata preservation, rename detection, ignore-case/whitespace options, color, regex exclusion, or network support.
- Exhaustive GNU diffutils CLI compatibility.
- Guaranteeing a globally shortest diff in every case.
- Comparing arbitrary binary data as lines.
- Unicode normalization or locale-dependent collation. Treat input as bytes.
- Any dependency on a GPL, noncommercial, or proprietary runtime implementation.

## 4. Initial command-line interface

```text
DIFF16 [-u] [-U N] [-o FILE] OLD NEW
DIFF16 -h
```

- **`-u`:** Unified format; default in v0.01. A plain non-unified output mode is not required.
- **`-U N`:** Number of unchanged context lines around changes. Range initially `0..99`, with default `3`; parse manually with overflow checking.
- **`-o FILE`:** Write to a named output file instead of standard output. If specified, never overwrite an input file. Refuse an output path that resolves to the same input file, including obvious drive/path aliases where detectable.
- **`-h`:** Print compact usage/help, with exit code `0`.
- **`--`:** Consider later if argument parsing warrants it; a literal filename beginning with `-` needs a documented escape convention if not initially supported.

DOS command tails have a practical length limit (~127 bytes), so do not design the CLI around long modern paths or many flags. Filenames should use the DOS path rules supplied by the environment; don't silently truncate long host-side names into 8.3 names.

**Exit status:** `0` no differences, `1` at least one difference, `2` error. If a write to stdout fails, return `2`, not `1`.

## 5. Unified-diff output contract

Example:

```diff
--- OLD.C
+++ NEW.C
@@ -2,3 +2,3 @@
 unchanged
-old text
+new text
 unchanged
```

### Requirements

- Use `---` and `+++` labels derived from command-line filenames, without timestamps in v0.01. Emit a header **only when differences exist**; no patch text for identical inputs.
- Emit standard hunk ranges in the forms `@@ -a,b +c,d @@`, with the conventional single-line and zero-length abbreviations where applicable. Test off-by-one boundaries explicitly.
- Prefix unified diff lines with a single ASCII space (context), minus (deletion), or plus (addition).
- Coalesce nearby changes when their context windows overlap; use deterministic hunk boundaries.
- Emit `\ No newline at end of file` for the relevant old/new line, exactly where the unified format requires it. A missing newline is part of the file's logical content and must not be ignored.
- Emit **LF** as the patch transport newline by default, even on DOS, so patches exchange cleanly with Unix tooling. Document that a byte-preserving transfer mode should be used over serial links. An optional `--dos-output` can be considered later, but must not alter patch semantics.
- Do not emit binary or Git extended patch headers (`diff --git`, `index`, `new file mode`). `patch16` can process these from other generators if its own spec requires it, but `diff16` need not generate them.
- Treat a filename with control characters or a form not safely representable in the selected header syntax as an error; do not fabricate a potentially misdirected patch.

**Interoperability target:** GNU `patch` on Linux must apply generated diffs; `patch16` must apply them once its unified-diff parser is implemented. Verify the exact range/EOF behavior with executable fixtures, not assumptions.

## 6. Text and line-ending semantics

### 6.1 Input text model

Represent a logical input line as `(bytes_without_terminator, termination_kind)` where termination kind is `LF`, `CRLF`, or `NONE`. A CR not immediately followed by LF remains part of line content, not a line terminator. Embedded NUL bytes cause a clear **binary/unsupported input** error in v0.01 rather than an ambiguous text patch.

**Default comparison policy:** Treat LF and CRLF line terminators as equivalent for *content comparison*, while still distinguishing a terminated final line from a line with **no final newline**. This makes it practical to compare source files copied between Linux and DOS.

This normalization is a defined **text-conversion mode**: applying the resulting patch to either file may preserve the target's local line-ending convention, as specified by `patch16`, rather than reproduce the exact old/new input bytes. The utility must document this limitation prominently.

### 6.2 Output policy

- Patch framing and emitted text lines use LF.
- Do not carry literal terminal CR bytes from CRLF lines into patch output under the default mode.
- Preserve meaningful CR bytes that are not part of CRLF.
- A distinct future `--strict-eol` mode may treat CRLF versus LF as changes, but is not required for v0.01; specify how such diffs could be faithfully applied before adding the option.
- Very long lines must not overflow fixed buffers or be silently truncated.

## 7. Algorithm exploration

### 7.1 Constraints

An XT may have limited conventional RAM remaining after DOS, program, stacks, and runtime. The code must not require `O(N*M)` RAM for two files with `N` and `M` lines. File size and line count must not be silently limited to 64 KiB or 65,535 lines merely because `int`, `size_t`, or near pointers are 16-bit.

### 7.2 Candidate strategies

| Strategy | Memory | Quality / complexity | Assessment |
|---|---|---|---|
| Full LCS matrix | `O(N*M)` | Straightforward but rapidly impractical | Reject |
| Myers shortest-edit-path | `O(N+M)` worst-case for frontier; trace memory can be worse | Often fast, good patches | Consider only with strict bounds |
| Linear-space divide-and-conquer (Myers/Hirschberg style) | `O(N+M)` for indices/frontiers, plus recursion/working sets | Good edit scripts; nontrivial disk-backed indexing | Preferred research direction |
| Block/anchor-based matching with bounded windows | Bounded memory and disk I/O | May be nonminimal in adversarial cases | Acceptable fallback |
| Simple common-prefix/suffix plus replacement of middle | Tiny memory | Valid but potentially huge patch | Guaranteed correctness fallback |

**Recommended approach:** A staged algorithm with an explicitly bounded memory budget:

1. Scan each file once and build a disk-backed line index containing line start offset, byte length, terminator flag, and a 32-bit/64-bit content hash or checksum. Use byte-for-byte verification on any hash match before treating lines as equal; hashes are only accelerators.
2. Remove equal prefixes and suffixes through indexed line comparison.
3. Attempt a bounded-memory diff algorithm on the remaining ranges, with well-defined limits on frontier arrays/recursion and disk I/O.
4. If limits are reached, emit a **correct but possibly nonminimal** remove/add hunk for the unresolved region, preserving available context. Never fail over to an edit script that has not been verified.
5. Merge/format hunks in a separate emitter stage.

**Explore before final implementation:** Whether a linear-space algorithm with file-backed line indices performs acceptably on an 8088; benchmark both time and RAM. It is legitimate for v0.01 to choose a simpler bounded algorithm if it produces valid interoperable patches.

### 7.3 Avoid one particularly dangerous optimization

Two lines with the same hash are **not necessarily equal**. The implementation must confirm equality by reading their bytes (or use an exact representation). A hash collision must never produce a silently incorrect patch.

## 8. Storage and memory architecture

Suggested modules (not mandatory filenames):

```text
src/
  main.c           argument parsing and top-level orchestration
  lines.c          line scanner and normalized comparison
  index.c          bounded, file-backed line index
  compare.c        matching algorithm and bounded fallback
  hunks.c          context/coalescing and range calculation
  unified.c        unified-diff writer
  temp.c           temporary-file lifecycle and cleanup
  output.c         safe byte/string/integer output; no *printf
  error.c          diagnostics and exit statuses
```

### File offsets and counts

- DOS 16-bit `int` is insufficient for file positions, line counts, or file sizes. Define explicitly sized types and check arithmetic for overflow.
- DOS seek APIs commonly expose **signed 32-bit offsets**, implying a practical <2 GiB file-position bound for ordinary files. v0.01 may set a smaller tested bound but must report unsupported sizes rather than wrap. Neither DOS filesystem nor cluster size limits alone define the maximum file the tool can process.
- Index entries may be serialized to temporary disk files and accessed with checked seeks; don't require contiguous near-heap storage.
- Work within the three `libc16` memory models as supported by the project; select the smallest model that meets performance and index-buffer requirements. **No model choice should be assumed without compilation and profiling.**
- Set explicit runtime budgets (e.g. a configurable working-set target on the order of tens of KiB) after measuring DOS memory availability. Avoid deep uncontrolled recursion and large automatic stack arrays.

### Temporary files and cleanup

- Select temp location via documented policy (`TMP`/`TEMP`, then current directory if writable); check free space and handle failures.
- Use collision-resistant names, checked exclusive creation where the DOS API permits it, and clean up on ordinary failure paths. Document that a crash/power loss can leave temporary files.
- Never write to, rename, truncate, or delete either input. Output named by `-o` should be created safely and not overwrite inputs. The diff is a read-only operation.
- Unavailable temp storage should cause either a documented lower-quality **memory-bounded** fallback or a clear error; no silent truncation.

## 9. `libc16` integration and API gaps

Start with a call-site inventory against the exact pinned `libc16` version; don't presume that missing POSIX APIs need to be added. Likely needs include:

- `open`, `read`, `write`, `close`, `lseek` or equivalent buffered `stdio` APIs; ensure binary-mode reads so CRLF normalization belongs to `diff16`, not the CRT.
- `malloc`/`free` only for bounded working sets, not full-file loading.
- Basic DOS error reporting and `errno` (if available), checked file creation, and cleanup.
- Decimal integer-to-text conversion using existing `libc16_ultoa` or a project-local routine; use direct `write` rather than `fprintf`.

**If missing:** Define the minimal new API and behavior with tests; implement it **clean-room under MIT** in `libc16`. Source implementations from GPL diff/patch projects, Open Watcom, or other non-MIT code must **not** enter `libc16`. Reusable text formatting APIs should not be added unless more than one consumer needs them.

**Runtime release gate:** The produced binary and link map must prove it does not contain Open Watcom runtime members, and the pinned `libc16` release/archive must be reproducible and integrity-checked.

## 10. Correctness and testing

### 10.1 Host-side deterministic tests

Create independently authored fixtures for:

- Identical files, empty/empty, empty/nonempty, one-line changes, additions/deletions at beginning/end.
- One-to-many and many-to-one edits; adjacent versus separated hunks; `-U 0`, `-U 1`, `-U 3`, large allowed context.
- Repeated identical lines (ambiguous matching), reversed blocks, all lines changed, and deliberately dissimilar large files.
- LF, CRLF, mixed line endings, no final newline on either/both sides, CR without LF, embedded NUL rejection.
- Very long lines, files with more than 65,535 lines, files crossing 64 KiB boundaries, and large on-disk file sizes.
- Hash collision simulation or forced hash function reduction to exercise mandatory byte-level equality checks.
- Resource failures: unreadable input, full temp disk, output write error, temp-name collision, interrupted comparison, invalid options, arithmetic overflow.

### 10.2 Differential interoperability checks

For every fixture:

1. Run `DIFF16 OLD NEW` in DOSBox-X and/or QEMU; record output and exit status.
2. Independently validate unified-diff syntax and hunk counts/ranges.
3. Apply the output with a modern Unix `patch` against a copied `OLD` and compare normalized content and final-newline state with `NEW`.
4. Apply with `patch16` once available, under its documented line-ending policy, with the same verification.
5. Run GNU `diff -u` as a **black-box behavioral comparator**, not a bit-for-bit oracle. Different correct edit scripts are allowed for ambiguous inputs.
6. Where the bounded fallback is exercised, assert applicability and exact normalized output, even when the diff is larger.

Never claim “correct” based only on matching GNU's diff text. **Patch application and resulting file content are the primary oracle.**

### 10.3 Real DOS tests

- Boot GR-DOS on 8088/V20-compatible 86Box, and run on real hardware when available.
- Test with floppy-only operation (including 360 KB), hard-disk paths, low free conventional memory, and no temp disk space.
- Establish executable size and peak memory usage; report large-input elapsed times rather than imposing an arbitrary speed promise.
- Test under at least MS-DOS 3.3 and GR-DOS 4.00 where images/licenses permit.

## 11. Planned milestones

| Milestone | Deliverable / gate |
|---|---|
| **D0 — Specification & provenance** | MIT license, independent algorithm/format specification, source provenance ledger, test fixtures, toolchain/runtime pinning. |
| **D1 — I/O foundation** | Build/link 8086 DOS executable with `libc16`, correct text scanner, binary reads, integer/status output, error handling; no proprietary runtime. |
| **D2 — Valid basic unified diffs** | Deterministic correct diffs with baseline bounded-memory algorithm; tested zero/one/multiple-hunk and EOF behavior. |
| **D3 — Better matching at bounded cost** | Disk-backed index and measured bounded matching algorithm; verified fallback for worst cases; stress tests. |
| **D4 — `patch16` interoperability** | Cross-tool integration matrix with Linux and DOS patches, CRLF/LF policy, repeated-lines cases, large files. |
| **D5 — v0.01 release** | Documentation, source license and provenance, reproducible release, tested 8088/GR-DOS operation and archived test outputs. |

## 12. Release acceptance criteria

Release only when all of the following are true:

- Source is an original MIT implementation with documented provenance; no copied GPL, historical noncommercial, Schilling-authored, or proprietary implementation source.
- Any new `libc16` functionality is independently implemented and MIT licensed, passes its own model/ABI tests, and is released/pinned appropriately.
- `DIFF16.EXE` is 8086-compatible and links exclusively to the allowed `libc16` runtime and project code.
- Unified diffs apply with Unix `patch` and `patch16` on representative fixtures, reconstructing the intended normalized text.
- Identical files produce no patch and exit `0`; differences exit `1`; all genuine errors exit `2`.
- LF/CRLF policy and final-newline semantics are documented and verified.
- All out-of-memory, size, I/O, and temporary-storage failures are explicit and cannot yield silently wrong patches or damage input files.
- Executable size, memory usage, and performance on an 8088-class target have been measured and documented.

## 13. Open exploration questions

1. **Algorithm choice:** Which bounded-memory approach offers useful hunk quality on an XT for realistic C, assembly, and text files? Benchmarks should decide.
2. **Index representation:** Which on-disk format/record size works well with 16-bit DOS seeks and limited handles?
3. **Context buffering:** Can hunks be generated in a second pass from a compact edit-script spool instead of retaining many lines?
4. **Line endings:** Is normalization-only sufficient initially, or is an explicit strict mode worth implementing after interoperability tests?
5. **Temporary location:** How should an XT with only one floppy and insufficient temp space degrade? An explicit failure may be preferable to excessive writes or invalid results.
6. **Long lines:** Should line length be unbounded via chunked comparison and emission, or should v0.01 impose a clearly documented ceiling?
7. **CLI labels:** Should a future `--label` or `-L` option allow Linux-friendly `a/` and `b/` names while retaining DOS 8.3 input paths?
8. **Shared utilities:** Which helpers, if any, have enough general value to live in MIT-only `libc16`, versus remaining inside `diff16`?

## 14. Suggested first implementation spike

Build a small 8086 DOS program that compares two files using chunked binary reads, normalizes CRLF/LF at the **line parser** level, and emits a valid unified diff for a single changed region using a full-region replacement fallback. Prove DOSBox-X/86Box execution, output parseability, non-overwriting behavior, and ability to process files larger than memory. Only then invest in a better edit-script algorithm.

This ordering validates the essential architecture **before** spending time optimizing diff quality.
