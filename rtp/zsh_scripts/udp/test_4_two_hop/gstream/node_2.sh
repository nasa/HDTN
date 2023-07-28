#!/bin/sh
# path variables
config_files=$HDTN_RTP_DIR/config_files/udp/test_4_two_hop
hdtn_config=$config_files/hdtn_one_process.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --contact-plan=$config_files/contact_plan.json --hdtn-config-file=$hdtn_config &
one_process_pid=$!
sleep 10

sleep 360

pkill $one_process_pid