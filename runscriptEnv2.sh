#!/bin/bash
cd $HDTN_SOURCE_ROOT/common/regsvr
`python3 main.py` &
cd $HDTN_BUILD_ROOT/common/bpcodec/apps
s="-s"
n="-n"
size="1500"
dest="1"
./bpgen "$s" "$size" "$n" "$dest" &
cd $HDTN_BUILD_ROOT/module/ingress
./hdtn-ingress &
#cd $HDTN_BUILD_ROOT/module/egress
#./hdtn-egress &
PID1=`pgrep bpgen`
PID2=`pgrep hdtn-ingress`
#PID3=`pgrep hdtn-egress`
sleep 10
(kill $PID1)
(kill -INT $PID2)
#(kill -INT $PID3)
#(kill -INT $PID4)
kill $(pgrep -f 'python3 main.py')





