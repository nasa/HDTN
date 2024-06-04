#!/bin/bash

for j in hdtn-one-process hdtn-ingress hdtn-egress-async hdtn-storage bpsink-async  bpgen-async bpreceivefile bpsendfile hdtn-router telem_cmd_interface udp-delay-sim encap-repeater bprecv_stream bpsend_stream
do
for i in `pidof $j`
do
 kill -SIGINT $i
done
done

sleep 6

for j in hdtn-one-process hdtn-ingress hdtn-egress-async hdtn-storage bpsink-async bpgen-async bpreceivefile bpsendfile hdtn-router telem_cmd_interface udp-delay-sim encap-repeater bprecv_stream bpsend_stream

do
for i in `pidof $j`
do
 kill -9 $i
done
done
