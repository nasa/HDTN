#!/bin/bash

while [ `du -b received/ | awk '{print $1}'` -lt 36300845631 ]
do
sleep 10
done
sleep 5

for i in `pidof bpreceivefile`
do
 kill -SIGINT $i
done

for i in `pidof hdtn-one-process`
do
 kill -SIGINT $i
done
