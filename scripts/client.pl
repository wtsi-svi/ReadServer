#! /usr/bin/perl

# perl script for sending asynchronous requests to the server
#     e.g.: cat test.in | perl client.pl --method [get(default)|kmermatch|gt] --output [count|reads|all(default)|samples] --endpoint http://host.ip.address:port/ <--kmer 34 --skip 10 <--pos 100> > <--threads 20>
# The test.in file should contain one query sequence(/kmer) per line

use strict;
use HTTP::Async;
use Data::Dumper;
use Getopt::Long;

require HTTP::Request;

my $max_concurrency = 20;
my $output = "all";
my $method = "get";
my $kmer  =34;
my $skip = 10;
my $pos = 100;
my $endpoint = "http://172.0.0.1:3900/";

GetOptions("threads=i" => \$max_concurrency,
           "kmer=i" => \$kmer,
           "output=s" => \$output,
           "endpoint=s" => \$endpoint,
           "method=s" => \$method,
           "skip=i" => \$skip,
           "pos=i" => \$pos);

my %opts = ();
$opts{"timeout"} = 900;
my $async = HTTP::Async->new(%opts);
my $count = 0;
my $url_base = $endpoint . $method . "?query=";

while ( my $seq = <> ) {
    chomp $seq;
    my $url = $url_base . $seq . "&output=" . $output;

    if ( $method eq "kmermatch" ) {
        $url = $url . "&kmer=" . $kmer . "&skip=" . $skip;
    }
    elsif ( $method eq "gt" ) {
        $url = $url . "&kmer=" . $kmer . "&skip=" . $skip . "&pos=" . $pos;
    }

    $async -> add( HTTP::Request->new( GET => $url ) );
    ++$count;

    while ( $count >= $max_concurrency ) {
        while ( $async->not_empty ) {
            if ( my $response = $async->next_response ) {
                --$count;
                if ( $response->is_success ) {
                    print $response->decoded_content, "\n";
                }
                else {
                    print STDERR Dumper $response;
                }
            }
            else {
                last;
            }
        }
    }
}

while ( $async->not_empty ) {
    if ( my $response = $async->next_response ) {
        if ( $response->is_success ) {
            print $response->decoded_content, "\n";
        }
        else {
            print STDERR Dumper $response;
        }
    }
}

