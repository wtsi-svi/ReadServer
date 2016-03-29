#! /usr/bin/perl

use strict;
use Getopt::Long;
use Parallel::ForkManager;

my $threads = 16;
GetOptions("threads=i" => \$threads);
my $pm = Parallel::ForkManager->new($threads);

DATA_LOOP:
while ( my $cmd = <> ) {
    chomp $cmd;
    print $cmd . "\n";
    $pm->start and next DATA_LOOP;
    qx($cmd);
    $pm->finish;
}

$pm->wait_all_children;
