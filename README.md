# ReadServer
Using reference-free compressed data structures to analyse thousands of human genomes simultaneously<br>

For more details, please see our [manuscript](http://biorxiv.org/content/early/2016/06/22/060186 "http://biorxiv.org/content/early/2016/06/22/060186").

<br>

### Before you start
---

This project includes most of its requirements as **git** submodules. However, many of the modules, as well as the main project itself, require a _C++11_ compatible compiler (GCC versions from **4.8.1** onwards should work). The following table contains a (non-exhaustive) list of Unix/Linux distributions and the version from which onwards they (should) have a compatible GCC version:

| Distribution             | Version  | Release date (YYYY-MM-DD) | GCC   |
|:-------------------------|:--------:|:-------------------------:|:-----:|
| Fedora                   | 19       | 2013-07-02                | 4.8.1 |
| Ubuntu                   | 13.10    | 2013-10-17                | 4.8.1 |
| openSUSE                 | 13.1     | 2013-11-19                | 4.8.1 |
| Mint                     | 16       | 2013-11-30                | 4.8.1 |
| Debian                   | 8.0      | 2015-04-26                | 4.9.2 |
| Red Hat Enterprise Linux | RHEL-7.2 | 2015-11-19                | 4.8.5 |
| CentOS                   | 7-1511   | 2015-12-14                | 4.8.5 |
| Scientific Linux         | 7.2      | 2016-02-05                | 4.8.5 |
| FreeBSD                  | 10.3     | 2016-04-04                | 4.8.4 |

If you install this software on a system that has an incompatible version set as default compiler, make sure to set the **_CC_** and **_CXX_** shell variables to the location of the binaries of a _C++11_ compatible compiler and export them in your shell:

```sh
export CC='/usr/local/gcc_4_9/bin/gcc-4.9.1'
export CXX='/usr/local/gcc_4_9/bin/g++-4.9.1'
```

You might also have to add the path to the corresponding _'lib'_ directories to your **_LIBRARY_PATH_** and **_LD_LIBRARY_PATH_** variables:

```sh
export LD_LIBRARY_PATH='/usr/local/gcc_4_9/lib:/usr/local/gcc_4_9/lib64':$LD_LIBRARY_PATH
export LIBRARY_PATH='/usr/local/gcc_4_9/lib:/usr/local/gcc_4_9/lib64':$LIBRARY_PATH
```

<br>

> **NOTE:**
> <br>
> _In case you are planning to compile and run the project in a HPC/Farm/Cluster environment, make sure that you compile on a system with the **minimum consensus resource set** that is available on **ALL** nodes in your computing environment! Some components in this project (e.g. 'RocksDB') optimise themselves for the hardware found on the machine they are compiled on. For instance, if the compiling system has **SSE 4.2** instruction set the final project will only run on machines with that instruction set._

<br>

We tested installation & compilation on a freshly set up _Ubuntu 16.04 LTS_ operating system and found the following required packages not to be included in the standard installation:

* cmake
* automake
* libtool
* texi2html
* texinfo
* docbook2x
* zlib1g-dev
* libbz2-dev

On an Ubuntu system (or any other distribution using the **apt** package manager and compatible repositories) you can install them with:

```sh
sudo apt install cmake automake libtool texi2html texinfo docbook2x zlib1g-dev libbz2-dev
```

_(if you don't have administrator rights on your system please ask your IT for help)_

<br><br>

### Installation
---

You can get a clone of this repository by typing
```sh
git clone https://github.com/wtsi-svi/ReadServer
```
or
```sh
git clone git@github.com:wtsi-svi/ReadServer.git
```
in your commandline.

<br>

After you have cloned this project, change to the **git** project directory _(e.g. '/usr/local/ReadServer')_ and execute:

```sh
bash install_dependencies.sh
```

The script should automatically fetch all required submodules and compile them. Afterwards run:

```sh
make clean
make
make install
```

Should you encounter compilation errors when running **'make'**, add

    CC=<path_to_c++11_compatible_c_compiler_binary> CXX=<path_to_c++11_compatible_c++_compiler_binary>

to the **'make'** command, e.g.

```sh
make CC='/usr/local/gcc_4_9/bin/gcc-4.9.1' CXX='/usr/local/gcc_4_9/bin/g++-4.9.1'
```

 _(the paths should be identical to those set for the shell variables)_. 

<br>

Once installation is finished, change to the _'demo'_ sub directory and follow the instructions in **'README.md'** to build a demonstration population BWT ReadServer.
