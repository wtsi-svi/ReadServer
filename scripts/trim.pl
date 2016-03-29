#! /usr/bin/perl

use strict;
use Getopt::Long;

my $length = 100;
GetOptions("length=i" => \$length);

while(my $line = <>) {
    chomp $line;
    print substr($line, 0, $length) . "\n";
}
