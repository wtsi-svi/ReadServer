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
    cd libs
    mkdir include
    mkdir lib
    mkdir bin
    mkdir share
    cd ..
fi


# ReadServer repository
REP_DIR=`pwd`

cd libs # ReadServer/libs/
LIB_DIR=`pwd`

cd ../submodules/bamtools # ReadServer/submodules/bamtools
BAMTOOLS_DIR=`pwd`

cd ../ # ReadServer/submodules/

#
# bamtools  bfc  cppzmq  google-sparsehash libconfig  libzmq  mongoose  protobuf  rocksdb  sga
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
cp zmq.hpp ../../libs/include/
cd .. # ReadServer/submodules

cd google-sparsehash # ReadServer/submodules/google-sparsehash
if [ ! -d src ]
then
    init_submodules
    cd submodules/google-sparsehash # ReadServer/submodules/google-sparsehash
fi
./configure --prefix=$LIB_DIR && make && make install
cd .. # ReadServer/submodules 

cd libconfig # ReadServer/submodules/libconfig
if [ ! -d lib ]
then
    init_submodules
    cd submodules/libconfig # ReadServer/submodules/libconfig
fi
aclocal
automake --add-missing
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
git checkout readserver
cd .. # ReadServer/submodules

cd protobuf # ReadServer/submodules/protobuf
if [ ! -d src ]
then
    init_submodules
    cd submodules/protobuf # ReadServer/submodules/protobuf
fi
./autogen.sh
./configure --prefix=$LIB_DIR && make && make install
cd .. # ReadServer/submodules 

cd snappy # ReadServer/submodules/snappy
if [ ! -e "snappy.h" ]
then
	init_submodules
	cd submodules/snappy # ReadServer/submodules/snappy
fi
./autogen.sh
./configure --prefix=$LIB_DIR && make && make install
cd .. # ReadServer/submodules 

cd rocksdb # ReadServer/submodules/rocksdb
if [ ! -d db ]
then
    init_submodules
    cd submodules/rocksdb # ReadServer/submodules/rocksdb
fi
make static_lib
mv librocksdb.a ../../libs/lib
make shared_lib
mv librocksdb.so* ../../libs/lib
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


cd ${REP_DIR}
cat demo/build_bwt.sh | awk -v r_path=${REP_DIR} 'BEGIN{}{gsub(/INST_SRC_DIR="<path>"/,"INST_SRC_DIR=\"REPLACE_PATH\"",$0);gsub(/REPLACE_PATH/,r_path,$0);print $0}END{}' > demo/build_bwt.sh.tmp
mv demo/build_bwt.sh.tmp demo/build_bwt.sh
