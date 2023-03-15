#!/bin/bash 
cd $1
for i in *
do
 echo $i `sha256sum "$i" | awk '{print $1}'`  >> "$HDTN_SOURCE_ROOT/flight/checksums/checksums_$2_$3_$4"
done

