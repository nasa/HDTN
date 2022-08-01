#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/tests/config_files
gen_config=$config_files/outducts/bpgen_one_stcp_port4556.json

cd $HDTN_SOURCE_ROOT

# BP Gen
./build/common/bpcodec/apps/bpgen-async  --use-bp-version-7 --bundle-rate=0 --duration=0 --bundle-size=100000 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$gen_config
