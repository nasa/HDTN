#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_routing_test.json
sink1_config=$config_files/inducts/bpsink_one_tcpcl_port4557.json
sink2_config=$config_files/inducts/bpsink_one_tcpcl_port4558.json
gen_config=$config_files/outducts/bpgen_one_tcpcl_port4556.json

cd $HDTN_SOURCE_ROOT

# bpsink1
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:1.1 --inducts-config-file=$sink1_config &
bpsink1_PID=$!
sleep 3

# bpsink2
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink2_config &
bpsink2_PID=$!
sleep 3

#hdtn-one-process
./build/module/hdtn_one_process/hdtn-one-process --hdtn-config-file=$hdtn_config &
one_process_PID=$!
sleep 6

#Scheduler
./build/module/scheduler/hdtn-scheduler --contact-plan-file=contactPlan.json --hdtn-config-file=$hdtn_config &
scheduler_PID=$!
sleep 1

#Router -The default routing Algorithm is CGR Dijkstra
# use the option --use-mgr to use Multigraph Routing Algorithm 
./build/module/router/hdtn-router --contact-plan-file=contactPlan.json --hdtn-config-file=$hdtn_config &
router_PID=$!
sleep 5

# bpgen1
./build/common/bpcodec/apps/bpgen-async  --use-bp-version-7 --bundle-rate=100 --my-uri-eid=ipn:101.1 --dest-uri-eid=ipn:1.1 --outducts-config-file=$gen_config &
bpgen1_PID=$!
sleep 1

#bpgen2
./build/common/bpcodec/apps/bpgen-async --use-bp-version-7 --bundle-rate=100 --my-uri-eid=ipn:102.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$gen_config &
bpgen2_PID=$!
sleep 8


