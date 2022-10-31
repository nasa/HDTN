#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_SDIL.json

cd $HDTN_SOURCE_ROOT

# HDTN one process
./build/module/hdtn_one_process/hdtn-one-process --hdtn-config-file=$hdtn_config &
sleep 8

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32768.1 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32852.1 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32935.1 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6


./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32768.64 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32852.64 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32935.64 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32768.65 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32852.65 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32935.65 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32768.2047 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32852.2047 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6

./build/module/storage/hdtn-release-message-sender --release-message-type=start --dest-uri-eid-to-release-or-stop=ipn:32935.2047 --delay-before-send=3 --hdtn-config-file=$hdtn_config &
sleep 6


