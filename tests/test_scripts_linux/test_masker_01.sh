#!/bin/sh

# This script runs the generator (ipn:1.1) and HDTN node A (ipn:10.1) of the Masker Test scenario.
# It is suggested you run the test_masker_02.sh script immediately before this one, which runs
# the sink node C (ipn:2.1) and HDTN node B (ipn:11.1) of the scenario.
# The reason these two scripts are separate is to make it easy to run them in different terminal
# screens so that the log output of each HDTN node can be easily distinguished.
# 
# This script also requires a Router modified to send RouteUpdateHdr messages to Egress that associate the 
# pseudo-destination 102 with the next hop 11. This Router change is not committed but you can modify 
# the Router to call SendRouteUpdate(11, 102), either during startup or regularly at the end of ComputeAllRoutes().
#
# The contact plan masker_test.json--stored in module/router/contact_plans--contains the network topology
# for this scenario. All bundles are generated with the destination node C (ipn:2.1).
# In the default topology with no masking, node A will forward all bundles directly to C.
# With a Router compiled as described above and a ShiftingMasker configured to mask the destination EID of 
# all bundles with (EID + 100), node A will instead forward all bundles through B to C.
#
# HDTN must be compiled with ENABLE_MASKING set to ON in CMakeCache.txt. By default it is OFF.

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
