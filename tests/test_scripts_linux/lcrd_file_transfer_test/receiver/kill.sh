#!/bin/bash

for j in hdtn-one-process bpsendfile bpreceivefile hdtn-router
do
for i in `pidof $j`
do
 kill -SIGINT $i
done
done

sleep 6

for j in hdtn-one-process bpsendfile bpreceivefile hdtn-router
do
for i in `pidof $j`
do
 kill -9 $i
done
done

