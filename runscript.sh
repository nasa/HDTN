#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/tests/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json
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

#Egress
./build/module/egress/hdtn-egress-async --hdtn-config-file=$hdtn_config &
egress_PID=$!
sleep 3

#Routing
# CGR server
python3 ./pycgr/py_cgr_client.py &
sleep 1

#Router
./build/module/router/hdtn-router --contact-plan-file=contactPlan.json --dest-uri-eid=ipn:2.1 --hdtn-config-file=$hdtn_config &
router_PID=$!
sleep 1

#Ingress
./build/module/ingress/hdtn-ingress --hdtn-config-file=$hdtn_config  &
ingress_PID=$!
sleep 3

#storage 
./build/module/storage/hdtn-storage --hdtn-config-file=$hdtn_config &
storage_PID=$!
sleep 3


#Scheduler
./build/module/scheduler/hdtn-scheduler --contact-plan-file=contactPlan.json --hdtn-config-file=$hdtn_config &
scheduler_PID=$!
sleep 1

# bpgen1
./build/common/bpcodec/apps/bpgen-async --bundle-rate=100 --my-uri-eid=ipn:101.1 --dest-uri-eid=ipn:1.1 --duration=40 --outducts-config-file=$gen_config &
bpgen1_PID=$!
sleep 1

#bpgen2
./build/common/bpcodec/apps/bpgen-async --bundle-rate=100 --my-uri-eid=ipn:102.1 --dest-uri-eid=ipn:2.1 --duration=40 --outducts-config-file=$gen_config &
bpgen2_PID=$!
sleep 8

# cleanup
sleep 30
echo "\nkilling bpgen1..." && kill -2 $bpgen1_PID
sleep 2
echo "\nkilling bpgen2..." && kill -2 $bpgen2_PID
sleep 2
echo "\nkilling HDTN storage..." && kill -2 $storage_PID
sleep 2
echo "\nkilling HDTN ingress..." && kill -2 $ingress_PID
sleep 2
echo "\nkilling Router..." && kill -2 $router_PID
sleep 2
echo "\nkilling scheduler..." && kill -2 $scheduler_PID
sleep 2
echo "\nkilling egress..." && kill -2 $egress_PID
sleep 2
echo "\nkilling bpsink2..." && kill -2 $bpsink2_PID
sleep 2
echo "\nkilling bpsink1..." && kill -2 $bpsink1_PID
sleep 2
echo "\nkilling py_cgr server..." && pkill -9 -f py_cgr_client.py

