#end all tests
sshpass -p "temppwd" ssh rover@rover.local pkill -9 HdtnOneProcessMain
sshpass -p "temppwd" ssh rover@rover.local pkill -9 gst-launch-1.0
sshpass -p "temppwd" ssh rover@rover.local pkill -9 ffmpeg

sshpass -p "temppwd" ssh gateway@gateway.local pkill -9 HdtnOneProcessMain
sshpass -p "temppwd" ssh madrid@madrid.local pkill -9 HdtnOneProcessMain
sshpass -p "temppwd" ssh relay@relay.local pkill -9 HdtnOneProcessMain
sshpass -p "temppwd" ssh nsn@nsn.local pkill -9 HdtnOneProcessMain
echo "Ended all tests"
#start all tests
sshpass -p "temppwd" ssh gateway@gateway.local ./HDTN/rtp/zsh_scripts/ltp/lunanet_test/gateway_run.sh
sshpass -p "temppwd" ssh madrid@madrid.local ./HDTN/rtp/zsh_scripts/ltp/lunanet_test/madrid_run.sh
sshpass -p "temppwd" ssh relay@relay.local ./HDTN/rtp/zsh_scripts/ltp/lunanet_test/relay_run.sh
sshpass -p "temppwd" ssh nsn@nsn.local ./HDTN/rtp/zsh_scripts/ltp/lunanet_test/nsn_run.sh
sshpass -p "temppwd" ssh rover@rover.local ./HDTN/rtp/zsh_scripts/ltp/lunanet_test/rover_run.sh 

echo "Started test"