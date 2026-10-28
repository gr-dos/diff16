#!/usr/bin/perl
# Make a random pair of text files for diff16's tests.
#
# usage: gen.pl SEED OLD NEW
#
# Lines come from a small vocabulary, so that repeated and ambiguous
# lines are common; line endings are LF, CR LF or mixed, a lone CR may
# be inside a line, and either file may lack a final newline. NEW is OLD
# with random insertions, deletions, replacements and block moves.

use strict;
use warnings;

my ($seed, $old, $new) = @ARGV;
die "usage: gen.pl SEED OLD NEW\n" unless defined $new;
srand($seed);

my @vocab = map { "line $_" } 0 .. (2 + int(rand(30)));
push @vocab, '', 'x' x (1 + int(rand(300))), "cr\rinside", "\t tab", '}';

sub pick { $vocab[int(rand(@vocab))] }

my $size = int(rand(4)) == 0 ? int(rand(400)) : int(rand(40));
my @a = map { pick() } 1 .. $size;
my @b = @a;

my $edits = int(rand(8));
for (1 .. $edits) {
    my $op = int(rand(4));
    my $at = int(rand(@b + 1));
    my $len = 1 + int(rand(5));
    if ($op == 0) {
        splice(@b, $at, 0, map { pick() } 1 .. $len);
    } elsif ($op == 1) {
        splice(@b, $at, $len);
    } elsif ($op == 2) {
        splice(@b, $at, $len, map { pick() } 1 .. 1 + int(rand(5)));
    } else {
        my @blk = splice(@b, $at, $len);
        my $to = int(rand(@b + 1));
        splice(@b, $to, 0, @blk);
    }
}
@b = map { pick() } 1 .. int(rand(30)) if rand() < 0.05;

# line endings: per file LF, CR LF, or mixed
sub write_file {
    my ($name, @lines) = @_;
    my $style = int(rand(3));
    my $text = '';
    for my $l (@lines) {
        my $crlf = $style == 1 || ($style == 2 && rand() < 0.5);
        $text .= $l . ($crlf ? "\r\n" : "\n");
    }
    # no final newline, sometimes
    if (@lines && rand() < 0.3) {
        $text =~ s/\r?\n\z//;
    }
    open(my $fh, '>:raw', $name) or die "$name: $!\n";
    print $fh $text;
    close $fh;
}

write_file($old, @a);
write_file($new, @b);
