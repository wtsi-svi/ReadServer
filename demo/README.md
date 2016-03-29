This is an example for demonstrating building a ReadServer.

The test dataset is from the following paper:
http://www.nature.com/ng/journal/v47/n6/full/ng.3281.html

The public accession numbers for the lanes are in here:
http://www.nature.com/ng/journal/v47/n6/extref/ng.3281-S2.xlsx

A local copy of above file (converted to csv) could be found in ng.3281-S2.csv

0). Before building a ReadServer for the demo dataset, compile and install the project if you have not done so.
cd ..
bash install_dependencies.sh
make && make install

1). Build the BWT string and RocksDB.
cd demo
bash build_bwt.sh
