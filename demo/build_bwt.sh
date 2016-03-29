#!/bin/bash

set -e

RUN_DIR=`pwd`

SOURCE_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SOURCE_DIR/../libs/lib

if [ ! -e "$SOURCE_DIR/ng.3281-S2.csv" ] 
then
    echo "File ng.3281-S2.csv does not exist."
    exit 1
fi

cd $RUN_DIR

# Clean up first
rm -rf fastq

# Obtain a list of 50 samples.
grep ERR360 $SOURCE_DIR/ng.3281-S2.csv | head -n 50 | awk -F ',' '{print $3}' | sort | uniq > list_of_sample_names

# For each sample in the list, generate 2 urls for its fastq files.
while read sample
do
    echo "wget -q ftp://ftp.sra.ebi.ac.uk/vol1/fastq/ERR360/$sample/${sample}_1.fastq.gz" >> tmp.commands
    echo "wget -q ftp://ftp.sra.ebi.ac.uk/vol1/fastq/ERR360/$sample/${sample}_2.fastq.gz" >> tmp.commands
done < list_of_sample_names

# Download fastq files.
mkdir fastq
cd fastq
cat ../tmp.commands | perl $SOURCE_DIR/../scripts/parallel_run.pl --threads 8
cd .. && rm -f tmp.commands

# concat two fastq files for same sample into one file
cd fastq
while read sample
do
    echo "zcat ${sample}_1.fastq.gz ${sample}_2.fastq.gz | gzip -1 > ${sample}.fastq.gz && rm -f ${sample}_1.fastq.gz ${sample}_2.fastq.gz" >> tmp.commands
done < ../list_of_sample_names
cat tmp.commands | perl $SOURCE_DIR/../scripts/parallel_run.pl --threads 8
rm -f tmp.commands
cd ..

# Use BFC for in sample error correction
cd fastq
while read sample
do
    echo "$SOURCE_DIR/../submodules/bfc/bfc -s 5m -t 4 -Q ${sample}.fastq.gz | gzip -1 > ${sample}.ec.fastq.gz && rm -f ${sample}.fastq.gz" >> tmp.commands
done < ../list_of_sample_names
cat tmp.commands | perl $SOURCE_DIR/../scripts/parallel_run.pl --threads 8
rm -f tmp.commands
cd ..

# Create hash for sample names
$SOURCE_DIR/../libs/bin/create_samples_hash list_of_sample_names list_of_sample_hash

# Convert error corrected fastq into file which contain read in 1st line and sample name in 2nd.
cd fastq
while read sample
do
    echo "zcat ${sample}.ec.fastq.gz | sed 'N;s/\n/\t/' | awk -F '\t' '{print \$3 \"\t\" \$1}' | awk -F '.' '{print \$1}' | sed -e 's/\t/\n/g' | sed -e 's/^>//g' | perl $SOURCE_DIR/../scripts/trim.pl --length 100 > ${sample}.ec.fastq && rm -f ${sample}.ec.fastq.gz" >> tmp.commands
done < ../list_of_sample_names
cat tmp.commands | perl $SOURCE_DIR/../scripts/parallel_run.pl --threads 8
rm -f tmp.commands
cd ..

# Split the fastq files into smaller parts so that each individual one can be sorted
cd fastq
while read sample
do
    echo "split --lines=1000000 -a 4 ${sample}.ec.fastq ${sample}.ec.fastq.part && rm -f ${sample}.ec.fastq" >> tmp.commands
done < ../list_of_sample_names
cat tmp.commands | perl $SOURCE_DIR/../scripts/parallel_run.pl --threads 8
rm -f tmp.commands
cd ..

# RLO sort the sequence and convert samples names to its hash value.
cd fastq
ls *part* > list_of_fastq_files
while read file
do
    echo "$SOURCE_DIR/../libs/bin/rlosort_seq_and_convert_sample_names ${file} ../list_of_sample_hash ${file}.rlosorted && rm -f ${file}" >> tmp.commands
done < list_of_fastq_files
cat tmp.commands | perl $SOURCE_DIR/../scripts/parallel_run.pl --threads 8
rm -f tmp.commands list_of_fastq_files
cd ..

# Merge RLO sorted reads
cd fastq
ls *.rlosorted > list_of_rlosorted_files.fofn
perl $SOURCE_DIR/../scripts/merge_sorted_files.pl --merger $SOURCE_DIR/../libs/bin/merge_rlosorted_reads --parallel_runner $SOURCE_DIR/../scripts/parallel_run.pl --fofn list_of_rlosorted_files.fofn --output merged.rlosorted.reads
rm -f list_of_rlosorted_files.fofn
cd ..

# Build BWT from rlosorted deduped read set.
if [ ! -d bwt ]
then
    mkdir bwt
fi
cd bwt
awk '{if (NR%2!=0) print }' ../fastq/merged.rlosorted.reads > final
perl $SOURCE_DIR/../scripts/build_bwt.pl --builder $SOURCE_DIR/../libs/bin/sga --input final
cd ..

# Index the bwt file built as above
cd bwt
$SOURCE_DIR/../libs/bin/index_rlebwt final.bwt
cd ..

# Load the reads (key) and ids (value) into RocksDB
rm -rf rocksdb
mkdir rocksdb
$SOURCE_DIR/../libs/bin/load_data_into_rocksdb fastq/merged.rlosorted.reads rocksdb
