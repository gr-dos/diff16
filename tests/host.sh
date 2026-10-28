#!/bin/sh
#
# Test the host builds of diff16 (make host).
#
# The oracle is patch, not another diff: for every pair OLD NEW, the diff
# must be well formed (tests/check.pl), and GNU patch applied to OLD must
# give NEW, both with line endings normalized (diff16 compares CR LF and
# LF as equal and writes LF). The exit status must be 0 exactly when the
# normalized files are equal. The stress builds (diff16-tiny, -weak) are
# held to the same standard.
#
# Usage: tests/host.sh [DIR]        (default build/host)
# Environment: FUZZ (default 300) random pairs per build; KEEP=1 keeps
#              the work directory.

set -u

top=$(cd "$(dirname "$0")/.." && pwd)
dir=$(cd "${1:-$top/build/host}" && pwd)
FUZZ=${FUZZ:-300}
LC_ALL=C
export LC_ALL

work=$(mktemp -d "${TMPDIR:-/tmp}/diff16.XXXXXX")
pass=0
fail=0
cd "$work" || exit 2

ok()     { pass=$((pass + 1)); }
not_ok() { echo "FAIL: $*"; fail=$((fail + 1)); }

norm() { perl -pe 's/\r\n\z/\n/' "$1"; }
lines() { perl -ne "END { print \$. + 0 }" "$1"; }

# applies EXE OLD NEW [OPTIONS]: diff, check, patch, compare
applies() {
    exe=$1 old=$2 new=$3
    shift 3
    "$exe" "$@" "$old" "$new" > out.dif 2> err.txt
    rc=$?
    norm "$old" > a.norm
    norm "$new" > b.norm
    if cmp -s a.norm b.norm; then want=0; else want=1; fi
    if [ "$rc" != "$want" ]; then
        not_ok "$(basename "$exe") $* $old $new: exit $rc, expected $want: $(cat err.txt)"
        return 1
    fi
    if [ "$rc" = 0 ]; then
        if [ -s out.dif ]; then
            not_ok "$(basename "$exe") $old $new: output for equal files"
            return 1
        fi
        ok
        return 0
    fi
    if ! perl "$top/tests/check.pl" out.dif "$(lines a.norm)" "$(lines b.norm)"; then
        not_ok "$(basename "$exe") $* $old $new: malformed diff"
        return 1
    fi
    rm -f res.txt
    if ! patch -s -f --binary -o res.txt a.norm < out.dif > patch.log 2>&1; then
        not_ok "$(basename "$exe") $* $old $new: patch failed: $(head -3 patch.log)"
        return 1
    fi
    [ -f res.txt ] || : > res.txt
    if ! cmp -s res.txt b.norm; then
        not_ok "$(basename "$exe") $* $old $new: patched OLD is not NEW"
        return 1
    fi
    ok
}

# status EXPECTED COMMAND...: exit status only
status() {
    want=$1
    shift
    "$@" > out.txt 2> err.txt
    rc=$?
    if [ "$rc" = "$want" ]; then ok; else not_ok "$*: exit $rc, expected $want"; fi
}

D="$dir/diff16"
BUILDS="$dir/diff16 $dir/diff16-tiny $dir/diff16-weak"

# ------------------------------------------------------------------
# fixed cases

: > empty
printf 'one\n' > one
printf 'one' > one-nonl
printf 'one\r\n' > one-crlf
printf 'a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\np\n' > abc
sed 's/^c$/C/; s/^n$/N/' abc > abc-2   # two changes 10 lines apart
sed 's/^c$/C/; s/^i$/I/' abc > abc-3   # 5 apart
sed '1d' abc > abc-del1
sed '$d' abc > abc-dellast
{ echo new; cat abc; } > abc-ins1
{ cat abc; echo new; } > abc-inslast
perl -pe 's/\n/\r\n/' abc > abc-crlf
perl -pe 's/\n/\r\n/ if $. % 2' abc-2 > abc-mixed
printf 'a\rb\nc\n' > lonecr
printf 'a\nb\nc\n' > nolonecr
printf 'x\r' > endcr
printf 'x\n' > x
printf 'x\r\n' > xcrlf
printf 'same\nsame\nsame\nsame\n' > rep4
printf 'same\nsame\nother\nsame\nsame\nsame\n' > rep6
seq 1 20 > fwd
seq 20 -1 1 > rev
seq 1 100 | sed 's/^/a/' > all-a
seq 1 100 | sed 's/^/b/' > all-b
seq 1 70000 > big1
seq 1 70000 | sed 's/^35000$/changed/; s/^69999$/x/' > big2
perl -e 'print "y" x 100000, "\n", "z" x 70000, "\n"' > long1
perl -e 'print "y" x 100000, "\n", "z" x 69999, "q\n"' > long2
perl -e 'print "y" x 100000, "\n", "z" x 70000' > long3

for exe in $BUILDS; do
    for pair in "empty empty" "empty one" "one empty" "one one-nonl" \
        "one-nonl one" "one one-crlf" "one-crlf one-nonl" "one-nonl one-nonl" \
        "abc abc-2" "abc abc-3" "abc abc-del1" "abc abc-dellast" \
        "abc abc-ins1" "abc abc-inslast" "abc abc-crlf" "abc abc-mixed" \
        "abc-mixed abc" "lonecr nolonecr" "endcr x" "x endcr" "xcrlf endcr" \
        "rep4 rep6" "rep6 rep4" "fwd rev" "all-a all-b" "big1 big2" \
        "long1 long2" "long1 long3" "long3 long2"; do
        # shellcheck disable=SC2086
        set -- $pair
        applies "$exe" "$1" "$2"
        for u in 0 1 5 99; do
            applies "$exe" "$1" "$2" -U "$u"
        done
    done
done

# hunk boundaries, against expected text
expect() {
    if printf '%s\n' "$2" | cmp -s - "$1"; then ok; else
        not_ok "$1 differs from expected:"
        printf '%s\n' "$2" | diff - "$1" | head -20
    fi
}
"$D" abc abc-2 > o.txt
expect o.txt '--- abc
+++ abc-2
@@ -1,6 +1,6 @@
 a
 b
-c
+C
 d
 e
 f
@@ -11,6 +11,6 @@
 k
 l
 m
-n
+N
 o
 p'
"$D" -U 3 abc abc-3 > o.txt
expect o.txt '--- abc
+++ abc-3
@@ -1,12 +1,12 @@
 a
 b
-c
+C
 d
 e
 f
 g
 h
-i
+I
 j
 k
 l'
"$D" -U 0 abc abc-del1 > o.txt
expect o.txt '--- abc
+++ abc-del1
@@ -1 +0,0 @@
-a'
"$D" -U 0 abc abc-inslast > o.txt
expect o.txt '--- abc
+++ abc-inslast
@@ -16,0 +17 @@
+new'
"$D" one-nonl one-crlf > o.txt
expect o.txt '--- one-nonl
+++ one-crlf
@@ -1 +1 @@
-one
\ No newline at end of file
+one'
"$D" endcr x > o.txt
perl -0ne 'exit !/\n-x\r\n\\ No newline at end of file\n\+x\n\z/' o.txt \
    && ok || not_ok "a lone CR at the end of a file is not kept"
"$D" -U0 -- abc abc-2 > o.txt
expect o.txt '--- abc
+++ abc-2
@@ -3 +3 @@
-c
+C
@@ -14 +14 @@
-n
+N'

# ------------------------------------------------------------------
# options and errors

status 0 "$D" -h
status 0 "$D" -u one one
status 1 "$D" -u one empty
status 2 "$D"
status 2 "$D" one
status 2 "$D" one one one
status 2 "$D" -x one one
status 2 "$D" -U one one
status 2 "$D" -U 100 one one
status 2 "$D" -U -1 one one
status 2 "$D" -U 1x one one
status 2 "$D" -U 99999999999999999999 one one
status 1 "$D" -U99 one empty
status 2 "$D" one nosuchfile
status 2 "$D" nosuchfile one
status 2 "$D" . one
printf 'a\0b\n' > nul
status 2 "$D" one nul
status 2 "$D" nul one
grep -q 'binary' err.txt && ok || not_ok "no 'binary' message for a NUL"
cp one 'with space'
status 2 "$D" one 'with space'
cp one ./-dash
status 1 "$D" -- -dash empty
status 2 "$D" -dash empty

# -o: never an input file; removed after an error
cp abc keep1
cp abc-2 keep2
status 2 "$D" -o keep1 keep1 keep2
cmp -s keep1 abc && ok || not_ok "-o overwrote OLD"
status 2 "$D" -o keep2 keep1 keep2
cmp -s keep2 abc-2 && ok || not_ok "-o overwrote NEW"
ln -s keep1 alias1
status 2 "$D" -o alias1 keep1 keep2
cmp -s keep1 abc && ok || not_ok "-o through a link overwrote OLD"
status 1 "$D" -o out.d keep1 keep2
"$D" keep1 keep2 | cmp -s - out.d && ok || not_ok "-o differs from standard output"
status 0 "$D" -o out.d keep1 keep1
[ -f out.d ] && [ ! -s out.d ] && ok || not_ok "-o for equal files is not empty"
status 2 "$D" -o out.d one nul
[ ! -e out.d ] && ok || not_ok "-o file left after an error"
status 2 "$D" -o nosuchdir/out.d keep1 keep2
if [ -w /dev/full ]; then
    status 2 "$D" -o /dev/full keep1 keep2
    "$D" big1 big2 > /dev/full 2> err.txt
    [ $? = 2 ] && ok || not_ok "write error on standard output is not status 2"
fi

# temporary files: in TMP, and removed afterwards
mkdir tmpd
TMP=$work/tmpd "$dir/diff16-tiny" big1 big2 > o.txt
[ $? = 1 ] && ok || not_ok "diff16-tiny big1 big2 with TMP"
[ -z "$(ls tmpd)" ] && ok || not_ok "temporary files left in TMP: $(ls tmpd)"
TMP=$work/nosuchdir TEMP=$work/tmpd "$dir/diff16-tiny" big1 big2 | cmp -s - o.txt \
    && ok || not_ok "fallback from a bad TMP to TEMP"
chmod 555 tmpd
TMP=$work/tmpd TEMP=$work/tmpd "$dir/diff16-tiny" big1 big2 > o2.txt 2>/dev/null
cmp -s o.txt o2.txt && ok || not_ok "fallback to the current directory"
chmod 755 tmpd
ls D16*.TMP >/dev/null 2>&1 && not_ok "temporary files left in ." || ok

# the release build makes the same diff whatever the memory
"$D" big1 big2 > o1.txt
"$dir/diff16-tiny" big1 big2 > o2.txt
perl "$top/tests/check.pl" o2.txt && ok || not_ok "tiny big1 big2 malformed"

# ------------------------------------------------------------------
# random pairs

i=0
while [ "$i" -lt "$FUZZ" ]; do
    perl "$top/tests/gen.pl" "$i" old.txt new.txt
    for exe in $BUILDS; do
        if ! applies "$exe" old.txt new.txt -U $((i % 4)); then
            cp old.txt "fail-$i-old.txt"
            cp new.txt "fail-$i-new.txt"
        fi
    done
    i=$((i + 1))
done

echo "host: $pass passed, $fail failed"
if [ "$fail" = 0 ] && [ -z "${KEEP:-}" ]; then
    cd / && rm -rf "$work"
else
    echo "work directory: $work"
fi
[ "$fail" = 0 ]
