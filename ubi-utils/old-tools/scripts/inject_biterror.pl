#!/usr/bin/perl -w
#
# 2007 Frank Haverkamp <haver@vnet.ibm.com>
#
# Program for bit-error injection. I am sure that perl experts do it
# in 1 line. Please let me know how it is done right ;-).
#

use strict;
use warnings;
use Getopt::Long;
use Pod::Usage;

my $i;
my $help;
my $result;
my $offset = 0;
my $bitmask = 0x01;
my $in = "input.mif";
my $out = "output.mif";

$result = GetOptions ("offset=i"  => \$offset,    # numeric
		      "bitmask=o" => \$bitmask,   # numeric
		      "input=s"	  => \$in,	  # string
		      "output=s"  => \$out,       # string
		      "help|?"    => \$help) or pod2usage(2);

pod2usage(1) if $help;

my $buf;

open(my $in_fh, "<", $in)
  or die "Cannot open file $in: $!";
binmode $in_fh;

open(my $out_fh, ">", $out) or
  die "Cannot open file $out: $!";
binmode $out_fh;

$i = 0;
while (sysread($in_fh, $buf, 1)) {

	$buf = pack('C', unpack('C', $buf) ^ $bitmask) if ($i == $offset);
	syswrite($out_fh, $buf, 1) or
	  die "Cannot write to offset $offset: $!";
	$i++;
}

close $in_fh;
close $out_fh;

__END__

=head1 NAME

inject_biterrors.pl

=head1 SYNOPSIS

inject_biterror.pl [options]

=head1 OPTIONS

=over 8

=item B<--help>

Print a brief help message and exits.

=item B<--offset>=I<offset>

Byte-offset where bit-error should be injected.

=item B<--bitmask>=I<bitmask>

Bit-mask where to inject errors in the byte.

=item B<--input>=I<input-file>

Input file.

=item B<--output>=I<output-file>

Output file.

=back

=head1 DESCRIPTION

B<inject_biterrors.pl> will read the given input file and inject
biterrors at the I<offset> specified. The location of the biterrors
are defined by the I<bitmask> parameter.

=cut
