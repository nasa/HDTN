#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/tests/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1udp_port4556_egress1udp_port4558flowid2_0.8Mbps.json
sink_config=$config_files/inducts/bpsink_one_udp_port4558.json
gen_config=$config_files/outducts/bpgen_one_udp_port4556_0.5Mbps.json

cd $HDTN_SOURCE_ROOT

# registration server
python3 ./common/regsvr/main.py &
sleep 3

# bpsink
./build/common/bpcodec/apps/bpsink-async --inducts-config-file=$sink_config &
bpsink_PID=$!
sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --hdtn-config-file=$hdtn_config &
one_process_PID=$!
sleep 3

# bpgen
./build/common/bpcodec/apps/bpgen-async --bundle-rate=10 --flow-id=2 --outducts-config-file=$gen_config &
bpgen_PID=$!
sleep 10

# cleanup
echo "\nkilling bpgen..." && kill -2 $bpgen_PID
sleep 2
echo "\nkilling HDTN one process..." && kill -2 $one_process_PID
sleep 2
echo "\nkilling bpsink..." && kill -2 $bpsink_PID
sleep 2
echo "\nkilling registration server..." && pkill -9 -f main.py

