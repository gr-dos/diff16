#!/usr/bin/perl
# Check the syntax of a unified diff from diff16, independently of
# patch: the two header lines, then hunks whose ranges agree with their
# lines, in order and not overlapping, with only ' ', '-', '+' lines and
# '\ No newline at end of file' after the last line of a file. LF only.
#
# usage: check.pl DIFF [OLDLINES NEWLINES]
# Exits 0 if the diff is well formed, else prints why and exits 1.

use strict;
use warnings;

my ($file, $olines, $nlines) = @ARGV;
open(my $fh, '<:raw', $file) or die "$file: $!\n";
local $/;
my $text = <$fh>;
close $fh;

sub bad { print "check.pl: $file: @_\n"; exit 1 }

exit 0 if $text eq '';
bad('no final newline') unless $text =~ /\n\z/;
my @l = split /\n/, $text, -1;
pop @l;

bad('bad --- header') unless (shift(@l) // '') =~ /^--- \S+$/;
bad('bad +++ header') unless (shift(@l) // '') =~ /^\+\+\+ \S+$/;
bad('no hunks') unless @l;

my ($enda, $endb) = (0, 0);
my $i = 0;
while ($i < @l) {
    my $h = $l[$i++];
    $h =~ /^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@$/ or bad("bad hunk header '$h'");
    my ($as, $ac, $bs, $bc) = ($1, $2 // 1, $3, $4 // 1);
    # an empty range names the line before it
    my $a0 = $ac ? $as - 1 : $as;
    my $b0 = $bc ? $bs - 1 : $bs;
    bad("range ',1' should be left out: $h") if $h =~ /,1 /;
    bad("hunk out of order: $h") if $a0 < $enda || $b0 < $endb;
    my ($na, $nb, $last) = (0, 0, '');
    while ($i < @l && $l[$i] !~ /^@@ /) {
        my $x = $l[$i++];
        my $c = substr($x, 0, 1);
        if ($c eq ' ') { $na++; $nb++ }
        elsif ($c eq '-') { $na++ }
        elsif ($c eq '+') { $nb++ }
        elsif ($x eq '\\ No newline at end of file') {
            bad('misplaced no-newline marker') if $last eq '' || $last eq '\\';
            $c = '\\';
        }
        else { bad("bad line '$x'") }
        $last = $c;
    }
    bad("old count $na, header says $ac: $h") if $na != $ac;
    bad("new count $nb, header says $bc: $h") if $nb != $bc;
    $enda = $a0 + $ac;
    $endb = $b0 + $bc;
}
if (defined $nlines) {
    bad("old range ends past the file ($enda > $olines)") if $enda > $olines;
    bad("new range ends past the file ($endb > $nlines)") if $endb > $nlines;
}
exit 0;
