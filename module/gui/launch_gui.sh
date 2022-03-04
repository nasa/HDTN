#!/usr/bin/env bash

mkdir root
pushd root
    cp ../*.html ./
    cp ../*.js ./
    ../build/hdtn-gui
popd
