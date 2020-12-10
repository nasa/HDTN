#!/bin/bash

echo "Building HDTN"
mkdir -p -- "build"
pushd build
cmake ..
make
popd

echo "Done."
