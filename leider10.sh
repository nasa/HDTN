#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
#hdtn_config=$config_files/hdtn/hdtn_ingress1tcpclv4_port4556_egress1tcpclv4_port4558flowid2.json
hdtn_config=$config_files/hdtn/leider.json
hdtn_config2=$config_files/hdtn/leider2.json
#sink_config=$config_files/inducts/bpsink_one_tcpclv4_port4558.json
sink_config=$config_files/inducts/bpsink_two_tcpclv4_port4557_port4559.json
#sink_config2=$config_files/inducts/bpsink_one_tcpclv4_port4557.json
gen_config=$config_files/outducts/bpgen_one_tcpclv4_port4556.json


cd $HDTN_SOURCE_ROOT

# bpsink2
#./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:102.1 --inducts-config-file=$sink_config2 &
#bpsink2_PID=$!
#sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --hdtn-config-file=$hdtn_config --contact-plan-file=leider.json --leider="shifting" &
oneprocess_PID=$!
sleep 10

#./build/module/hdtn_one_process/hdtn-one-process --hdtn-config-file=$hdtn_config2 --contact-plan-file=leider.json &
#oneprocess_PID2=$!
#sleep 10


#bpgen (configure bundle-rate=0 to send bundles at high rate)
./build/common/bpcodec/apps/bpgen-async --use-bp-version-7 --bundle-rate=100 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --duration=40 --outducts-config-file=$gen_config &
bpgen_PID=$!
sleep 8

# cleanup
sleep 30
echo "\nkilling bpgen1..." && kill -2 $bpgen_PID
sleep 2
echo "\nkilling hdtn1..." && kill -2 $oneprocess_PID
#sleep 2
#echo "\nkilling hdtn2..." && kill -2 $oneprocess_PID2
#sleep 2
#echo "\nkilling bpsink1..." && kill -2 $bpsink1_PID
#sleep 2
#echo "\nkilling bpsink2..." && kill -2 $bpsink2_PID

