#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1stcp_port4556_egress1ltpdelayproxy_flowid2_50Mbps.json
sink_config=$config_files/inducts/bpsink_one_ltp_delaysim.json
gen_config=$config_files/outducts/bpgen_one_stcp_port4556.json

cd $HDTN_SOURCE_ROOT

#start los 17 seconds in because 1 second ltp ping will start los counter

#UDP delay Sim for sender 
./build/module/udp_delay_sim/udp-delay-sim  --remote-udp-hostname=localhost  --remote-udp-port=4558 --my-bound-udp-port=1114 --num-rx-udp-packets-buffer-size=10000 --max-rx-udp-packet-size-bytes=65000 --send-delay-ms=0 --los-start-ms=17000 --los-duration-ms=10000 &
sleep 3

#UDP Delay Sim for receiver 
./build/module/udp_delay_sim/udp-delay-sim --remote-udp-hostname=localhost --remote-udp-port=1113 --my-bound-udp-port=4556 --num-rx-udp-packets-buffer-size=100 --max-rx-udp-packet-size-bytes=65000 --send-delay-ms=0 --los-start-ms=17000 --los-duration-ms=10000 &
sleep 3

#Bpsink
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=contactPlanCutThroughMode_50Mbps.json --hdtn-config-file=$hdtn_config &
sleep 6

# Bpgen
./build/common/bpcodec/apps/bpgen-async --bundle-rate=100 --bundle-size=100000 --duration=40 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$gen_config &
sleep 8


