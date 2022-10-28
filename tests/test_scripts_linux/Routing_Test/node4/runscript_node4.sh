#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_node4_cfg.json
sink_config=$config_files/inducts/bpsink_one_stcp_port4560.json

cd $HDTN_SOURCE_ROOT

# bpsink1
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:200.1 --inducts-config-file=$sink_config &
sleep 3

#Egress
./build/module/egress/hdtn-egress-async --hdtn-config-file=$hdtn_config &
sleep 3

#Router
./build/module/router/hdtn-router --contact-plan-file=contactPlan.json --dest-uri-eid=ipn:200.1 --hdtn-config-file=$hdtn_config &
sleep 1

#Ingress
./build/module/ingress/hdtn-ingress --hdtn-config-file=$hdtn_config  &
sleep 3

#storage 
./build/module/storage/hdtn-storage --hdtn-config-file=$hdtn_config &
sleep 3

#Scheduler
./build/module/scheduler/hdtn-scheduler --contact-plan-file=contactPlan.json --hdtn-config-file=$hdtn_config &
sleep 1

