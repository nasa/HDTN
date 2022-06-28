#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/tests/config_files
hdtn_config=$config_files/hdtn/aws_arm_test.json
sink_config=$config_files/inducts/bpsink_one_tcpclv4_port4558.json
gen_config=$config_files/outducts/bpgen_one_tcpclv4_port4556.json

cd $HDTN_SOURCE_ROOT

#Use this script as an guide. Run each command in seperate console window. This will be a lot easier to read results

# BP Receive File
./build/common/bpcodec/apps/bpreceivefile --save-directory=received --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
bpreceive_PID=$!
sleep 3


# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --cut-through-only-test  --hdtn-config-file=$hdtn_config &
sleep 6


# BP Gen 
./build/common/bpcodec/apps/bpgen-async  --use-bp-version-7 --bundle-rate=0 --duration=0 --bundle-size=100000 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$gen_config &
bpsend_PID=$!
sleep 8


# cleanup or just run ./kill.sh or ctr^c on each window
sleep 55
echo "\nkilling bp send file..." && kill -2 $bpsend_PID
sleep 2
echo "\nkilling HDTN one process ..." && kill -2 $one_process_PID
sleep 2
echo "\nkilling bp receive file..." && kill -2 $bpreceive_PID

