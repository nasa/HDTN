# run this on gateway
# path variables
config_files=$HDTN_RTP_DIR/config_files/ltp_egress_node_sink_test
gen_config=$config_files/bpgen_node_1.json

cd $HDTN_SOURCE_ROOT

#bpgen
./build/common/bpcodec/apps/bpgen-async --bundle-size=10000 --bundle-rate=200 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:3.1 --outducts-config-file=$gen_config &
pid_gen=$!
sleep 8

sleep 360

pkill -2 $pid_gen