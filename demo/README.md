# ReadServer demo

This is an example for demonstrating how to build a ReadServer.

<br>
The test dataset is taken from the following paper:

[Wong VK et al. (2015) Nature Genetics 47, pp.632Ð639](http://www.nature.com/ng/journal/v47/n6/full/ng.3281.html "Phylogeographical analysis of the dominant multidrug-resistant H58 clade of Salmonella Typhi identifies inter- and intracontinental transmission events")

The public accession numbers for the lanes are in here: [XLSX](http://www.nature.com/ng/journal/v47/n6/extref/ng.3281-S2.xlsx "http://www.nature.com/ng/journal/v47/n6/extref/ng.3281-S2.xlsx"). The _'demo'_ directory contains a copy of the XLSX in CSV format (**_'ng.3281-S2.csv'_**) which is used for the building process.

<br>

### Installation

Before trying to build a ReadServer for the demo dataset, make sure that you compiled and installed the project. Check the **'README.md'** in the main project directory for instructions.

<br>

### Data processing

  - Build the BWT and RocksDB with:
    
    ```sh
    bash build_bwt.sh <start_step>-<end_step> <destination_directory>
    ```
    
    If you run **_'build_bwt.sh'_** without parameters it will display all steps available.
    
    <br>
    
  - If you just want to build the demo server with default settings _(i.e. building it for 10 samples)_ in a local directory run:
    
    ```sh
    mkdir DemoServer
    cd DemoServer
    bash ../build_bwt.sh 1-10 .
    ```
    
    This will build the server in _"<path_to_read_server_repository>/demo/DemoServer/SERVER"_.<br>Once the script has finished and the server is built run
    
    ```sh
    bash ../build_bwt.sh 11-11 .
    ```
    
    to configure it _(i.e. specify host IP-address & ports)_.<br><br>If you want to build a **smaller/larger** server using **less/more** samples, change the **_INST_SMP_** variable in **_'build_bwt.sh'_** to any number between **1-50**. You can also follow the building process and keep intermediate steps by setting **_INST_CLEANUP_** to **"0"**. Just keep in mind that this will create a large amount of data _[>100GB for 50 samples]_ on your harddrive so better not use this option for larger numbers of samples!

<br>

### Starting & stopping the server

  - Change to the _'SERVER'_ folder _(e.g. "<path_to_read_server_repository>/demo/DemoServer/SERVER")_ within the destination directory specified during the build process

  - Start the server with:
    ```sh
    bash start_demo_server.sh
    ```
  - You can stop the server again with
    ```sh
    bash exit_demo_server.sh
    ```
    if necessary.

<br>

### Play time

  - If the server is running, go to your web browser and try:
    
    ```
    http://host.ip.address:port/get?query=AAGCGCATACTCCCGCTGTACGTTACGGCGGGAGACCC&output=all
    ```
    
  - Or alternatively send multiple queries asynchronously:
    ```sh
    cd demo && cat test.in | perl ../scripts/client.pl --endpoint http://host.ip.address:port/ | less
    ```
