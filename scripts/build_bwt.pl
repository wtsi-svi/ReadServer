#! /usr/bin/perl
use strict;
use Getopt::Long;

my $bwt_builder = "";
my $input = "";
my $prefix = "final";
GetOptions("builder=s" => \$bwt_builder,
           "prefix=s" => \$prefix,
           "input=s" => \$input);

my $tmp_input = "$prefix.tmp";
unlink($tmp_input);
my $preprocess = "cat $input | grep -v 'N' >> $tmp_input && mv $tmp_input $prefix";
run($preprocess);

my $bwt = makeFMIndex($prefix); 

sub makeFMIndex {
    my($raw_file) = @_;
    my $fa_file = "$raw_file.fa";
    my $awk_cmd = qq(awk '{ print ">" NR "\\n" \$0 }');
    my $cmd1 = "cat $raw_file | $awk_cmd > $fa_file";
    run($cmd1);

    my $cmd2 = "$bwt_builder index -a ropebwt -t 4 --no-reverse $fa_file";
    run($cmd2);

    my $cmd3 = "rm -f $fa_file";
    run($cmd3);

    my $index_name;
    $index_name = "$raw_file.bwt";

    return $index_name;
}


sub run
{
    my($cmd) = @_;
    print "$cmd\n";
    qx($cmd);
}

