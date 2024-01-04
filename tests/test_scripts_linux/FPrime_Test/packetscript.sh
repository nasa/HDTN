#!/bin/sh

# A runscript for a three node network containing instances of BpSendPacket, BpReceivePacket, and HDTN.
# In this case, the BpReceivePacket app is configured to listen for incoming bundles from a TCPCLv4 induct, extract the payload, and send it in a UDP packet on port 7132.
# The BpSendPacket app is configured to expect raw data framed in STCP, extract the payload, bundle it, and send it over TCPCLv4 to ipn:2.1.
# The script was written to test Fprime integration. However, you can run it with any UDP (or STCP) client and server, including
# the Python files in this directory which were written test this script.
# As an example, you can run this script, run udpreceiver.py, and then run udpsender.py and watch the output of the udpreceiver. This shows data can be converted
# from a UDP packet (or STCP framed raw data) into bundled data sent over any convergence layer, and vice versa.

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1tcpclv4_port4556_egress1tcpclv4_port4558flowid2.json
sink_config=$config_files/inducts/bpsink_one_tcpclv4_port4558.json
gen_config=$config_files/outducts/bpgen_one_tcpclv4_port4556.json

# use UDP or STCP
send_config=$config_files/inducts/bpgen_one_stcp_port4560.json
# send_config=$config_files/inducts/bpgen_one_udp_port4560.json
receive_config=$config_files/outducts/udp7132.json

cd $HDTN_SOURCE_ROOT

# receivepacket
./build/common/bpcodec/apps/bpreceivepacket --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config --packet-outducts-config-file=$receive_config &
bpsink_PID=$!
sleep 3

# HDTN one process
# use the option --use-unix-timestamp when using a contact plan with unix timestamp
# use the option --use-mgr to use Multigraph Routing Algorithm (the default routing Algorithm is CGR Dijkstra) 
./build/module/hdtn_one_process/hdtn-one-process --hdtn-config-file=$hdtn_config --contact-plan-file=contactPlanCutThroughMode.json &
oneprocess_PID=$!
sleep 10

# sendpacket
./build/common/bpcodec/apps/bpsendpacket --use-bp-version-7 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$gen_config --packet-inducts-config-file=$send_config &
bpgen_PID=$!
sleep 8

# cleanup
sleep 120
echo "\nkilling bpgen1..." && kill -2 $bpgen_PID
sleep 2
echo "\nkilling egress..." && kill -2 $oneprocess_PID
sleep 2
echo "\nkilling bpsink2..." && kill -2 $bpsink_PID

