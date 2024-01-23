#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1stcp_port4556_egress1bpencap_unixsocket_flowid2.json
sink_config=$config_files/inducts/bpsink_one_bpencap_unixsocket2.json
gen_config=$config_files/outducts/bpgen_one_stcp_port4556.json

cd $HDTN_SOURCE_ROOT

#encap repeater
./build/module/encap_repeater/encap-repeater --stream-name-0=/tmp/bp_local_encap_socket --stream-init-0=create --stream-name-1=/tmp/bp_local_encap_socket2 --stream-init-1=open --queue-size=5 --encap-packet-type=bp &
sleep 3

#Bpsink
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=contactPlanCutThroughMode_unlimitedRate.json --hdtn-config-file=$hdtn_config &
sleep 6

# Bpgen
./build/common/bpcodec/apps/bpgen-async --bundle-rate=0 --bundle-size=100000 --duration=20 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$gen_config &
sleep 8


