# ReadServer
Using reference-free compressed data structures to analyse thousands of human genomes simultaneously<br>**(1000 Genomes ReadServer)**

<br>

### Before you start
---

This project includes most of its requirements as **git** submodules. However, many of the modules, as well as the main project itself, require a _C++11_ compatible compiler (GCC versions from **4.8.1** onwards should work). If you install this software on a system that has an incompatible version set as default compiler, make sure to set the **_CC_** and **_CXX_** shell variables to the location of the binaries of a _C++11_ compatible compiler and export them in your shell:

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

---
> **NOTE:**
> <br>
> _In case you are planning to compile and run the project in a HPC/Farm/Cluster environment, make sure that you compile on a system with the **minimum consensus resource set** that is available on **ALL** nodes in your computing environment! Some components in this project _(e.g. 'RocksDB')_ optimise themselves for the hardware found on the machine they are compiled on. For instance, if the compiling system has **_SSE 4.2_** instruction set the final project will only run on machines with that instruction set._
---

<br>

### Installation
---

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

Once installation is finished, change to the _'demo'_ sub directory and follow the instructions in **'README.md'** to build a demonstration ReadServer.
