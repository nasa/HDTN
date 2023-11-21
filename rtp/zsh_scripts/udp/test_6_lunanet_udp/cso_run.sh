#!/bin/sh
#run this on cso 
# path variables
config_files=$HDTN_RTP_DIR/config_files/luna_net
hdtn_config=$config_files/hdtn_one_process_node_6.json
contact_plan=$HDTN_RTP_DIR/config_files/contact_plans/LunaNetContactPlanNodeIDs.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=$contact_plan --hdtn-config-file=$hdtn_config &
one_process_pid=$!
sleep 10

sleep 360

pkill $one_process_pid