#!/bin/sh

# This script runs the generator (ipn:1.1) and HDTN node A (ipn:10.1) of the Masker Test scenario.
# It is suggested you run the masker_test_02.sh script immediately before this one, which runs
# the sink (ipn:2.1) and HDTN node B (ipn:11.1) of the Masker Test scenario.
# The reason these two scripts are separate is to make it easy to run them in different terminal
# screens so that the log output of each HDTN node can be easily distinguished.

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/masker_test_01.json
gen_config=$config_files/outducts/bpgen_one_tcpclv4_port4556.json


cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --hdtn-config-file=$hdtn_config --contact-plan-file=masker_test.json --masker="shifting" &
oneprocess_PID=$!
sleep 10

#bpgen (configure bundle-rate=0 to send bundles at high rate)
./build/common/bpcodec/apps/bpgen-async --use-bp-version-7 --bundle-rate=100 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --duration=40 --outducts-config-file=$gen_config &
bpgen_PID=$!
sleep 8

# cleanup
sleep 30
echo "\nkilling bpgen1..." && kill -2 $bpgen_PID
sleep 2
echo "\nkilling hdtn1..." && kill -2 $oneprocess_PID
