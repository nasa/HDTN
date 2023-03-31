#!/bin/bash
mkdir checksums
cd $1

TIME=$(date "+%Y.%m.%d-%H.%M.%S")
for i in *
do
 echo $i `sha256sum "$i" | awk '{print $1}'`  >> "$HDTN_SOURCE_ROOT/tests/test_scripts_linux/lcrd_file_transfer_test/sender/checksums/checksums_SENDER_$TIME"
done

