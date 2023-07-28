#!/bin/sh
#run this on madrid
# path variables
config_files=$HDTN_RTP_DIR/config_files/ltp_egress_node_sink_test/
hdtn_config=$config_files/hdtn_one_process_node_2.json
contact_plan=$config_files/contact_plan.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=$contact_plan --hdtn-config-file=$hdtn_config &
one_process_pid=$!
sleep 10

sleep 360

pkill $one_process_pid