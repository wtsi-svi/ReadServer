#! /usr/bin/perl

use strict;
use Getopt::Long;

my $merger = "";
my $fofn = "";
my $out = "";
my $parallel_runner = "";
GetOptions("fofn=s" => \$fofn,
           "output=s" => \$out,
           "parallel_runner=s" => \$parallel_runner,
           "merger=s" => \$merger);


my @files;
open FILE, "<$fofn";
@files = <FILE>;
close FILE;

my @tmp_files;
foreach my $f ( @files ) {
    chomp $f;
    push @tmp_files, $f;
}

my $cmds = "tmp.commands";
my $num_of_files = scalar(@tmp_files);
my $step = 0;

while ( $num_of_files > 1 ) {
    my $count = 0;
    my @new_tmp_files;

    my $rm_cmd = "rm -f $cmds";
    run($rm_cmd);

    if ( $num_of_files % 2 > 0 ) {
        my $last_file = $tmp_files[$num_of_files-1];
        my $new_file = "tmp.$step.$count";
        my $mv_cmd = "mv $last_file $new_file";
        run($mv_cmd);
        push @new_tmp_files, $new_file;
    }

    for ( my $i=0; $i<$num_of_files-1; $i=$i+2 ) {
        $count++;

        my $first_file = $tmp_files[$i];
        my $second_file = $tmp_files[$i+1];
        my $new_file = "tmp.$step.$count";

        my $merge_cmd = "$merger $first_file $second_file $new_file && rm -f $first_file $second_file";
        my $wr_cmd = "echo \"$merge_cmd\" >> $cmds";
        run($wr_cmd);
        push @new_tmp_files, $new_file;
    }

    my $run_cmd = "cat $cmds | perl $parallel_runner --threads 8";
    run($run_cmd);

    $step++;
    @tmp_files = @new_tmp_files;
    $num_of_files = scalar(@tmp_files);
}

if ( $num_of_files == 1 ) {
    my $last_file = @tmp_files[0];
    my $mv_cmd = "mv $last_file $out";
    run($mv_cmd);
}

my $rm_cmd = "rm -f $cmds";
run($rm_cmd);

sub run {
    my($cmd) = @_;
    print "$cmd\n";
    qx($cmd);
}
