#!/bin/bash
INSTALL_DIR=$(dirname "$0")/external.install
BUILD_DIR=$(dirname "$0")/build
SRC_DIR=$(dirname "$0")/external
HOME_DIR=$(dirname "$0")

if [ ! -d external.install ]; then
    rm -fr $SRC_DIR
    mkdir $SRC_DIR
    pushd $SRC_DIR > /dev/null
    echo "Building CivetWeb ..."
    pushd $HOME_DIR/module/gui/civetweb > /dev/null
    make clean slib WITH_CPP=1 WITH_WEBSOCKET=1 >> civet.make.out
    mkdir -p $INSTALL_DIR/lib
    cp *.so $INSTALL_DIR/lib
    cp *.so.* $INSTALL_DIR/lib
    mkdir -p $INSTALL_DIR/include
    cp include/* $INSTALL_DIR/include/
    popd > /dev/null
    echo "Done"
    popd > /dev/null
fi

mkdir build
pushd build > /dev/null
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8
popd > /dev/null


