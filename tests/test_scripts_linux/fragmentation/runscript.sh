#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if [ -z "$HDTN_SOURCE_ROOT" ]; then
    echo "Please set HDTN_SOURCE_ROOT"
    exit 1
fi

src="$SCRIPT_DIR/data-src"
dest="$SCRIPT_DIR/data-dest"
fname="test-file.bin"

mkdir -p "$src" "$dest"
rm -fv "$dest/$fname"

dd if=/dev/urandom of="$src/$fname" bs=1024 count=1024

$HDTN_SOURCE_ROOT/build/common/bpcodec/apps/bpreceivefile \
    --save-directory="$dest" \
    --my-uri-eid=ipn:2.1 \
    --inducts-config-file="$SCRIPT_DIR/bpreceivefile-in.json" \
    --custody-transfer-outducts-config-file="$SCRIPT_DIR/bpreceivefile-out.json" &

$HDTN_SOURCE_ROOT/build/module/hdtn_one_process/hdtn-one-process \
    --port-number=8086 \
    --hdtn-config-file="$SCRIPT_DIR/hdtnA.json" \
    --contact-plan-file="$SCRIPT_DIR/contact_plan.json" &

$HDTN_SOURCE_ROOT/build/module/hdtn_one_process/hdtn-one-process \
    --port-number=8087 \
    --hdtn-config-file="$SCRIPT_DIR/hdtnB.json" \
    --contact-plan-file="$SCRIPT_DIR/contact_plan.json" &

$HDTN_SOURCE_ROOT/build/common/bpcodec/apps/bpsendfile \
	--max-bundle-size-bytes=964 \
	--file-or-folder-path="$src" \
	--recurse-directories-depth=2 \
	--my-uri-eid=ipn:3.1 \
	--dest-uri-eid=ipn:2.1 \
	--bundle-priority=1 \
	--bundle-lifetime-milliseconds=30000 \
	--outducts-config-file="$SCRIPT_DIR/bpsendfile-out.json" \
	--cla-rate=5600000 \
	--custody-transfer-inducts-config-file="$SCRIPT_DIR/bpsendfile-in.json" &

sleep 10
pkill -P $$
wait
