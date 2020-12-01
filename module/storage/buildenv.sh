#!/usr/bin/env bash

mkdir lib 2> /dev/null
pushd lib > /dev/null 2> /dev/null
echo "Fetching c-blosc ..."
git clone https://github.com/Blosc/c-blosc
pushd c-blosc > /dev/null 2> /dev/null
mkdir build
pushd build > /dev/null 2> /dev/null
echo "Building c-blosc ..."
cmake -DCMAKE_INSTALL_PREFIX=../../ ..
cmake --build .
cmake --build . --target install
popd > /dev/null 2> /dev/null
popd > /dev/null 2> /dev/null
# Pull libraries up a level, so that we're not going to lib/lib
mv lib/* ./
rmdir lib
popd > /dev/null 2> /dev/null
echo "Done."
