#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if [ -z "$HDTN_SOURCE_ROOT" ]; then
    echo "Please set HDTN_SOURCE_ROOT"
    exit 1
fi

$HDTN_SOURCE_ROOT/build/common/bpcodec/apps/bpgen-async \
    --outducts-config-file="$SCRIPT_DIR/bpgen.json" \
    --my-uri-eid=ipn:1.1 \
    --bundle-size=600 \
    --bundle-rate=1000000 \
    --dest-uri-eid=ipn:2.1 \
    --force-disable-custody &

$HDTN_SOURCE_ROOT/build/module/hdtn_one_process/hdtn-one-process \
    --hdtn-config-file="$SCRIPT_DIR/hdtn.json" \
    --contact-plan-file="$SCRIPT_DIR/contact-plan.json" &

$HDTN_SOURCE_ROOT/build/common/bpcodec/apps/bpsink-async \
    --inducts-config-file="$SCRIPT_DIR/bpsink.json" \
    --my-uri-eid="ipn:2.1" &

function cleanup {
    echo "Exiting..."
    kill $(jobs -p)
    sleep 1
    kill -9 $(jobs -p)
}

trap cleanup SIGINT SIGTERM EXIT
wait
