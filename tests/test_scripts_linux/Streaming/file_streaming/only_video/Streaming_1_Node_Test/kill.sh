#!/bin/bash

for j in hdtn-one-process bpsend_stream bprecv_stream
do
for i in `pidof $j`
do
 kill -SIGINT $i
done
done

sleep 6

for j in hdtn-one-process bpsend_stream bprecv_stream
do
for i in `pidof $j`
do
 kill -9 $i
done
done
