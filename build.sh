#!/bin/bash

echo "Update git submodules"
git submodule update
git submodule foreach git checkout master
git status

echo "Building HDTN"
mkdir -p -- "build"
pushd build
cmake ..
make
popd

echo "Done."
