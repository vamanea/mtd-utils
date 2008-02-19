#!/usr/bin/perl -w

sub usage;

my @tests = ("mkvol_basic", "mkvol_bad", "mkvol_paral", "rsvol",
	     "io_basic", "io_read", "io_update", "io_paral");

if (not defined @ARGV) {
	usage();
	exit;
}

foreach (@ARGV) {
	-c or die "Error: $_ is not character device\n";
}

my $dev;
foreach $dev (@ARGV) {
	foreach (@tests) {
		print "Running: $_ $dev";
		system "./$_ $dev" and die;
		print "\tSUCCESS\n";
	}
}

sub usage
{
	print "Usage:\n";
	print "$0 <UBI device 1> <UBI device 2> ...\n";
}
