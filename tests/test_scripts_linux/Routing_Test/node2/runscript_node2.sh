#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_node2_cfg.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process  --hdtn-config-file=$hdtn_config &
sleep 10

#Router
./build/module/router/hdtn-router --contact-plan-file=contactPlan_RoutingTest.json --dest-uri-eid=ipn:200.1 --hdtn-config-file=$hdtn_config &
sleep 1

#scheduler
./build/module/scheduler/hdtn-scheduler --contact-plan-file=contactPlan_RoutingTest.json --hdtn-config-file=$hdtn_config &
sleep 1

