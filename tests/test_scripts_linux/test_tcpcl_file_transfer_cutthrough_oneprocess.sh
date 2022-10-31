#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json
sink_config=$config_files/inducts/bpsink_one_tcpcl_port4558.json
gen_config=$config_files/outducts/bpgen_one_tcpcl_port4556.json

cd $HDTN_SOURCE_ROOT

# BP Receive File
./build/common/bpcodec/apps/bpreceivefile --save-directory=received --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config &
bpreceive_PID=$!
sleep 3

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --hdtn-config-file=$hdtn_config &
one_process_PID=$!
sleep 6

#Scheduler
./build/module/scheduler/hdtn-scheduler --contact-plan-file=contactPlanCutThroughMode.json --hdtn-config-file=$hdtn_config &
scheduler_PID=$!
sleep 5

# BP Send File 
./build/common/bpcodec/apps/bpsendfile --max-bundle-size-bytes=4000000 --file-or-folder-path=test.txt --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$gen_config &
bpsend_PID=$!
sleep 8

# cleanup
sleep 30
echo "\nkilling bp send file..." && kill -2 $bpsend_PID
sleep 2
echo "\nkilling HDTN one process ..." && kill -2 $one_process_PID
sleep 2
echo "\nkilling HDTN scheduler ..." && kill -2 $scheduler_PID
sleep 2
echo "\nkilling bp receive file..." && kill -2 $bpreceive_PID



