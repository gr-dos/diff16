#!/bin/sh
#
# Test diff16.exe, and the stress builds dtiny.exe and dweak.exe, under
# DOSBox-X.
#
# Fixtures are made on the host, one batch file runs every DOS step, and
# the results are checked on the host afterwards. Step N records the exit
# status in RN.TXT and standard output in ON.TXT (DOS cannot redirect
# standard error, so messages are not checked). rc.bat uses goto rather
# than "if errorlevel N echo ... >file" because the DOSBox-X shell opens
# the redirection even when the test is false.
#
# Checks: diff16.exe's output is byte for byte the host build's for the
# same arguments (the output depends only on the input, not on memory);
# the stress builds' diffs are well formed and patch OLD into NEW; exit
# statuses; -o never replaces an input, however it is named; temporary
# files are made in TMP and removed. big1/big2 have 140,000 lines
# between them, more keys than conventional memory holds, so the release
# build spills to a temporary file too.
#
# Usage: tests/dos.sh DIFF16.EXE DTINY.EXE DWEAK.EXE HOST-DIFF16
# Environment: DOSBOX (default dosbox-x), FUZZ (default 30) random pairs,
#              KEEP=1 to keep the work directory.

set -u

top=$(cd "$(dirname "$0")/.." && pwd)
exe=$1 tiny=$2 weak=$3 host=$4
DOSBOX=${DOSBOX:-dosbox-x}
FUZZ=${FUZZ:-30}
LC_ALL=C
export LC_ALL

command -v "$DOSBOX" >/dev/null || { echo "$DOSBOX not found" >&2; exit 2; }
for f in "$exe" "$tiny" "$weak" "$host"; do
    [ -f "$f" ] || { echo "$f not found" >&2; exit 2; }
done

work=$(mktemp -d "${TMPDIR:-/tmp}/dosdiff.XXXXXX")
pass=0
fail=0
cd "$work" || exit 2
mkdir dos hostrun
cp "$exe" dos/diff16.exe
cp "$tiny" dos/dtiny.exe
cp "$weak" dos/dweak.exe

ok()     { pass=$((pass + 1)); }
not_ok() { echo "FAIL: $*"; fail=$((fail + 1)); }

# ------------------------------------------------------------------
# fixtures, in dos/ with 8.3 names

cd dos || exit 2
: > empty.txt
printf 'one\n' > one.txt
printf 'one' > onenonl.txt
printf 'one\r\n' > onecrlf.txt
seq 1 16 | sed 's/^/l/' > abc.txt
sed 's/^l3$/C/; s/^l14$/N/' abc.txt > abc2.txt
perl -pe 's/\n/\r\n/ if $. % 2' abc2.txt > abcmix.txt
printf 'a\rb\nc\n' > lonecr.txt
printf 'x\r' > endcr.txt
printf 'x\n' > x.txt
seq 1 20 > fwd.txt
seq 20 -1 1 > rev.txt
seq 1 70000 > big1.txt
seq 1 70000 | sed 's/^35000$/changed/; s/^69999$/x/; 100a\
inserted' > big2.txt
perl -e 'print "y" x 100000, "\n", "z" x 70000, "\n"' > long1.txt
perl -e 'print "y" x 100000, "\n", "z" x 69999, "q\n"' > long2.txt
# not nul.txt: DOS opens the NUL device whatever the extension
printf 'a\0b\n' > zero.txt
cp abc.txt keep1.txt
cp abc2.txt keep2.txt
mkdir tmpd

PAIRS="empty.txt:one.txt one.txt:empty.txt one.txt:onenonl.txt
onenonl.txt:onecrlf.txt one.txt:onecrlf.txt abc.txt:abc2.txt abc.txt:abcmix.txt
abcmix.txt:abc.txt lonecr.txt:abc.txt endcr.txt:x.txt x.txt:endcr.txt
fwd.txt:rev.txt big1.txt:big2.txt long1.txt:long2.txt"
i=0
while [ "$i" -lt "$FUZZ" ]; do
    n=$(printf '%02d' "$i")
    perl "$top/tests/gen.pl" "$((i + 5000))" "p${n}a.txt" "p${n}b.txt"
    PAIRS="$PAIRS p${n}a.txt:p${n}b.txt"
    i=$((i + 1))
done

cat > rc.bat <<'EOF'
@echo off
if errorlevel 3 goto rc3
if errorlevel 2 goto rc2
if errorlevel 1 goto rc1
echo rc0 >c:\r%1.txt
goto end
:rc1
echo rc1 >c:\r%1.txt
goto end
:rc2
echo rc2 >c:\r%1.txt
goto end
:rc3
echo rc3 >c:\r%1.txt
:end
EOF

# Steps: "N|PROGRAM ARGS|EXPECTED-STATUS|KIND" where KIND is
#   same    output must equal the host build's for ARGS
#   apply   output must patch OLD into NEW (the last two ARGS)
#   none    status only
steps=steps.lst
: > $steps
n=0
step() { n=$((n + 1)); echo "$n|$1|$2|$3" >> $steps; }

for p in $PAIRS; do
    a=${p%%:*} b=${p#*:}
    step "diff16 $a $b" x same
    step "dtiny $a $b" x apply
    step "dweak $a $b" x apply
done
step "diff16 -U 0 abc.txt abc2.txt" 1 same
step "diff16 -U0 big1.txt big2.txt" 1 same
step "diff16 -U 99 abc.txt abcmix.txt" 1 same
step "diff16 -u one.txt one.txt" 0 same
step "diff16 abc.txt abcmix.txt" 1 same
step "diff16 -h" 0 none
step "diff16" 2 none
step "diff16 one.txt" 2 none
step "diff16 -x one.txt one.txt" 2 none
step "diff16 -U 100 one.txt one.txt" 2 none
step "diff16 one.txt nosuch.txt" 2 none
step "diff16 zero.txt one.txt" 2 none
step "diff16 one.txt tmpd" 2 none
# -o: never an input, however named; no file left after an error
step "diff16 -o keep1.txt keep1.txt keep2.txt" 2 none
step "diff16 -o KEEP2.TXT keep1.txt keep2.txt" 2 none
step "diff16 -o .\\keep1.txt keep1.txt keep2.txt" 2 none
step "diff16 -o c:keep1.txt keep1.txt keep2.txt" 2 none
step "diff16 -o c:\\tmpd\\..\\keep2.txt keep1.txt keep2.txt" 2 none
step "diff16 -o keep1.txtx keep1.txt keep2.txt" 2 none
step "diff16 -o out.dif keep1.txt keep2.txt" 1 none
step "diff16 -o bad.dif one.txt zero.txt" 2 none

cd ..

# The batch file. Steps that use TMP set it themselves.
{
    echo '@echo off'
    echo 'set TMP='
    echo 'set TEMP='
    while IFS='|' read -r num cmd want kind; do
        echo "$cmd >o$num.txt"
        echo "call rc $num"
    done < dos/$steps
    echo 'set TMP=c:\tmpd'
    echo 'dtiny big1.txt big2.txt >tmp1.txt'
    echo 'call rc tmp1'
    echo 'set TMP=c:\nosuch'
    echo 'set TEMP=c:\tmpd'
    echo 'dtiny big1.txt big2.txt >tmp2.txt'
    echo 'call rc tmp2'
    echo 'set TEMP='
    echo 'set TMP='
} | sed 's/$/\r/' > dos/steps.bat

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 1200 \
    "$DOSBOX" -defaultconf -silent -nogui -nomenu \
    -set "cpu cycles=max" \
    -c "mount c \"$work/dos\"" -c "c:" \
    -c "steps.bat" -c "exit" > dosbox.log 2>&1

# ------------------------------------------------------------------
# checks

# a file from the DOS side, whatever case DOSBox-X gave its name
dosfile() {
    f=$(find dos -maxdepth 1 -iname "$1" | head -1)
    echo "${f:-dos/$1}"
}

rcof() { tr -d ' \r' < "$(dosfile "r$1.txt")" 2>/dev/null | sed 's/^rc//'; }

norm() { perl -pe 's/\r\n\z/\n/' "$1"; }
lines() { perl -ne "END { print \$. + 0 }" "$1"; }

cp dos/*.txt hostrun/ 2>/dev/null
rm -f hostrun/[OR]*.TXT

while IFS='|' read -r num cmd want kind; do
    got=$(rcof "$num")
    # shellcheck disable=SC2086
    set -- $cmd
    prog=$1
    shift
    if [ "$want" = x ]; then
        a=$(eval echo "\${$(($# - 1))}") b=$(eval echo "\${$#}")
        norm "dos/$a" > a.norm
        norm "dos/$b" > b.norm
        if cmp -s a.norm b.norm; then want=0; else want=1; fi
    fi
    if [ "$got" != "$want" ]; then
        not_ok "step $num ($cmd): exit status ${got:-missing}, expected $want"
        continue
    fi
    out=$(dosfile "o$num.txt")
    case $kind in
    same)
        (cd hostrun && "$host" "$@" > ../host.out 2>/dev/null)
        if cmp -s host.out "$out"; then ok; else
            not_ok "step $num ($cmd): DOS output differs from the host build's"
        fi ;;
    apply)
        if [ "$got" = 0 ]; then
            [ -s "$out" ] && not_ok "step $num ($cmd): output for equal files" || ok
            continue
        fi
        a=$(eval echo "\${$(($# - 1))}") b=$(eval echo "\${$#}")
        norm "dos/$a" > a.norm
        norm "dos/$b" > b.norm
        if ! perl "$top/tests/check.pl" "$out" "$(lines a.norm)" "$(lines b.norm)"; then
            not_ok "step $num ($cmd): malformed diff"
            continue
        fi
        rm -f res.txt
        if patch -s -f --binary -o res.txt a.norm < "$out" > patch.log 2>&1 \
            && { [ -f res.txt ] || : > res.txt; } && cmp -s res.txt b.norm; then
            ok
        else
            not_ok "step $num ($cmd): patch did not give NEW"
        fi ;;
    none)
        ok ;;
    esac
done < dos/$steps

cmp -s dos/keep1.txt dos/abc.txt && ok || not_ok "-o replaced keep1.txt"
cmp -s dos/keep2.txt dos/abc2.txt && ok || not_ok "-o replaced keep2.txt"
[ -z "$(find dos -maxdepth 1 -iname bad.dif)" ] && ok || not_ok "-o file left after an error"
(cd hostrun && "$host" keep1.txt keep2.txt > ../host.out)
cmp -s host.out "$(dosfile out.dif)" && ok || not_ok "-o output differs"

[ "$(rcof tmp1)" = 1 ] && ok || not_ok "dtiny with TMP: exit status $(rcof tmp1)"
[ "$(rcof tmp2)" = 1 ] && ok || not_ok "dtiny with a bad TMP: exit status $(rcof tmp2)"
(cd hostrun && "$host" big1.txt big2.txt > ../host.out)
for f in tmp1.txt tmp2.txt; do
    cmp -s host.out "$(dosfile $f)" && ok || not_ok "$f differs from the host build's diff"
done
left=$(cd dos && { ls tmpd; ls | grep -i "^d16.*\.tmp$"; })
[ -z "$left" ] && ok || not_ok "temporary files left: $left"

echo "dos: $pass passed, $fail failed"
if [ "$fail" = 0 ] && [ -z "${KEEP:-}" ]; then
    cd / && rm -rf "$work"
else
    echo "work directory: $work"
fi
[ "$fail" = 0 ]
