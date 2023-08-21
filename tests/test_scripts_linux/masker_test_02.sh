#!/bin/sh

# See masker_test_01.sh for explanation of this script.

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/leider.json
hdtn_config2=$config_files/hdtn/leider2.json
sink_config=$config_files/inducts/bpsink_two_tcpclv4_port4557_port4559.json
gen_config=$config_files/outducts/bpgen_one_tcpclv4_port4556.json


cd $HDTN_SOURCE_ROOT

# bpsink1
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
bpsink1_PID=$!
sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --hdtn-config-file=$hdtn_config2 --contact-plan-file=leider.json &
oneprocess_PID2=$!
sleep 10

# cleanup
sleep 60
echo "\nkilling hdtn2..." && kill -2 $oneprocess_PID2
sleep 2
echo "\nkilling bpsink1..." && kill -2 $bpsink1_PID
