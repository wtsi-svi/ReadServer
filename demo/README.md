# ReadServer demo

This is an example for demonstrating building a ReadServer.
The test dataset is from the following paper:

    http://www.nature.com/ng/journal/v47/n6/full/ng.3281.html

The public accession numbers for the lanes are in here:
    
    http://www.nature.com/ng/journal/v47/n6/extref/ng.3281-S2.xlsx

A local copy of above file (converted to csv) could be found in ng.3281-S2.csv

### Installation

Before building a ReadServer for the demo dataset, compile and install the project if you have not done so.
```sh
cd .. && bash install_dependencies.sh && make clean && make && make install
```

### Data processing

  - Build the BWT string and RocksDB.
    ```sh
    cd demo && bash build_bwt.sh
    ```

  - Edit server.cfg service.cfg and service_samples.cfg as appropriate.

### Starting server

  - Edit start_demo_server.sh and exit_demo_server.sh as appropriate.

  - Start the server.
    ```sh
    bash start_demo_server.sh
    ```

### Play time

  - Go to your web browser and try:

    http://host.ip.address:port/get?query=AAGCGCATACTCCCGCTGTACGTTACGGCGGGAGACCC&output=all

  - Or alternatively send multiple queries asynchronously:
    ```sh
    cd demo && cat test.in | perl ../scripts/client.pl --endpoint http://host.ip.address:port/ | less
    ```
