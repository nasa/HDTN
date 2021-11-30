#!/usr/bin/env bash
INSTALL_DIR='pwd'/external.install


echo "Building CivetWeb ..."
pushd civetweb-1.15
make
sudo make install

