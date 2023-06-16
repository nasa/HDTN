#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_node2_cfg.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=contactPlan_RoutingTest.json  --hdtn-config-file=$hdtn_config &
sleep 10

#router
./build/module/scheduler/hdtn-router --contact-plan-file=contactPlan_RoutingTest.json --hdtn-config-file=$hdtn_config &
sleep 1

