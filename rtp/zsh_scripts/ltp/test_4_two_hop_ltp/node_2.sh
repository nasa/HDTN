
config_files=$HDTN_RTP_DIR/config_files/two_hop_ltp
contact_plan=$HDTN_RTP_DIR/config_files/two_hop_ltp/contact_plan
hdtn_config=$config_files/node_2.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process  --contact-plan-file=$contact_plan --hdtn-config-file=$hdtn_config &
pid_hdtn=$!

sleep 400

kill -2 $pid_hdtn