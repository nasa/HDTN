# TCPCL\_2Nodes\_Test
* Tests the connection ability using TCPCL withn the HDTN protocol

## How to use
* Within the "receiver" container, run ./runscript\_receiver\_tcpcl.sh
* Then wait about 3 seconds
* When you are done waiting for 3 seconds, within the "sender" container run ./runscript\_sender\_tcpcl.sh
* Wait about 10 seconds for the sender and receiver programs to initialize and operate
* You can then use tcpdump to observe the network traffic from:
    * the sender container
    * the receiver container
    * the host machine running the container
* Running tcpdump on these different locations will show a different perspective on network traffic which may be beneficial for your testing


