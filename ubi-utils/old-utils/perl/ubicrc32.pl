#!/usr/bin/perl -w

# Subroutine crc32(): Calculates the CRC on a given string.

{
    my @table = ();

    # @brief Calculate CRC32 for a given string.
    sub crc32
    {
	unless (@table) {
	    # Initialize the CRC table
	    my $poly = 0xEDB88320;
	    @table = ();

	    for my $i (0..255) {
		my $c = $i;

		for my $j (0..7) {
		    $c = ($c & 1) ? (($c >> 1) ^ $poly) : ($c >> 1);
		}
		$table[$i] = $c;
	    }
	}
	my $s = shift;		# string to calculate the CRC for
	my $crc = shift;	# CRC start value

	defined($crc)
	    or $crc = 0xffffffff; # Default CRC start value

	for (my $i = 0; $i < length($s); $i++) {
	    $crc = $table[($crc ^ ord(substr($s, $i, 1))) & 0xff]
		^ ($crc >> 8);
	}
	return $crc;
    }
}

sub crc32_on_file
{
    my $file = shift;

    my $crc32 = crc32('');
    my $buf = '';
    my $ret = 0;

    while ($ret = read($file, $buf, 8192)) {
	$crc32 = crc32($buf, $crc32);
    }
    defined($ret)
	or return undef;
    printf("0x%x\n", $crc32);
}


# Main routine: Calculate the CRCs on the given files and print the
# results.

{
    if (@ARGV) {
	while (my $path = shift) {
	    my $file;
	    open $file, "<", $path
		or die "Error opening '$path'.\n";
	    
	    &crc32_on_file($file)
		or die "Error reading from '$path'.\n";
	    close $file;
	}
    } else {
	&crc32_on_file(\*STDIN)
	    or die "Error reading from stdin.\n";
    }
}
