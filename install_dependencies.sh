#!/bin/bash

set -e

init_submodules () {
    cd ../.. # ReadServer/
    git submodule init
    git submodule update
}

if [ ! -d libs ]
then
    mkdir libs # create ReadServer/libs/
fi

cd libs # ReadServer/libs/
LIB_DIR=`pwd`

cd ../submodules/bamtools # ReadServer/submodules/bamtools
BAMTOOLS_DIR=`pwd`

cd ../ # ReadServer/submodules/

#
# bamtools  bfc  cppzmq  google-sparsehash  libzmq  mongoose  protobuf  rocksdb  sga
#

cd bamtools # ReadServer/submodules/bamtools
if [ ! -d src ]
then
    init_submodules
    cd submodules/bamtools # ReadServer/submodules/bamtools
fi
if [ ! -d build ]
then
    mkdir build
fi
cd build # ReadServer/submodules/bamtools/build
cmake .. && make
cd ../.. # ReadServer/submodules 

cd bfc # ReadServer/submodules/bfc
if [ ! -d tex ]
then
    init_submodules
    cd submodules/bfc # ReadServer/submodules/bfc
fi
make
cd .. # ReadServer/submodules

cd cppzmq # ReadServer/submodules/cppzmq
if [ ! -e "README" ]
then
    init_submodules
    cd submodules/cppzmq # ReadServer/submodules/cppzmq
fi
cd .. # ReadServer/submodules

cd google-sparsehash # ReadServer/submodules/google-sparsehash
if [ ! -d src ]
then
    init_submodules
    cd submodules/google-sparsehash # ReadServer/submodules/google-sparsehash
fi
./configure --prefix=$LIB_DIR && make && make install
cd .. # ReadServer/submodules 

cd libzmq # ReadServer/submodules/libzmq
if [ ! -d src ]
then
    init_submodules
    cd submodules/libzmq # ReadServer/submodules/libzmq
fi
./autogen.sh
./configure --prefix=$LIB_DIR && make && make install
cd .. # ReadServer/submodules 

cd mongoose # ReadServer/submodules/mongoose
if [ ! -e "mongoose.h" ]
then
    init_submodules
    cd submodules/mongoose # ReadServer/submodules/mongoose
fi
cd .. # ReadServer/submodules

cd protobuf # ReadServer/submodules/protobuf
if [ ! -d src ]
then
    init_submodules
    cd submodules/protobuf # ReadServer/submodules/protobuf
fi
./autogen.sh
./configure --prefix=$LIB_DIR && make && make check && make install
cd .. # ReadServer/submodules 

cd rocksdb # ReadServer/submodules/rocksdb
if [ ! -d db ]
then
    init_submodules
    cd submodules/rocksdb # ReadServer/submodules/rocksdb
fi
make shared_lib
cd .. # ReadServer/submodules 

cd sga # ReadServer/submodules/sga
if [ ! -d src ]
then
    init_submodules
    cd submodules/sga # ReadServer/submodules/sga
fi
cd src
./autogen.sh
./configure --prefix=$LIB_DIR --with-sparsehash=$LIB_DIR --with-bamtools=$BAMTOOLS_DIR
make && make install
cd ../.. # ReadServer/submodules
