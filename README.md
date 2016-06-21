# ReadServer
Using reference-free compressed data structures to analyse thousands of human genomes simultaneously
**(1000 Genomes ReadServer)**


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

Should you encounter compilation errors when running one of these commands, add

    CC=<path_to_c++11_compatible_c_compiler_binary> CXX=<path_to_c++11_compatible_c++_compiler_binary>

to the **'make'** command(s), e.g.

```sh
make CC='/usr/local/gcc_4_9/bin/gcc-4.9.1' CXX='/usr/local/gcc_4_9/bin/g++-4.9.1'
```

 _(the paths should be identical to those set for the shell variables)_. 

Once installation is finished, change to the _'demo'_ sub directory and modify the following section in **'build_bwt.sh'** script as specified:

```sh
##################################################################################
#	ONLY MAKE CHANGES HERE, DO NOT ALTER OTHER PARTS OF THE SCRIPT
##################################################################################
INST_SRC_DIR="<path>" # Replace by absolute path to 'ReadServer' project directory
INST_CLEANUP=1        # "INST_CLEANUP"=="1" => script will delete previous steps
                      # "INST_CLEANUP"=="0" => script won't
INST_PTHREADS=1       # Number of parallel threads used
INST_SMP=10           # Number of samples for which to build the ReadServer
                      # Anything up to 50 should work.
##################################################################################
```

You should then be able to build a demonstration ReadServer by running the script with:

```sh
bash build_bwt.sh <start_step>-<end_step> <destination_directory>
```

Running it without parameters will display the build steps available. For more information check the _'README.md'_ in the _'demo'_ directory.
