# File Transfer Test
* Tests the ability to send a file using HDTN
* This test supports sending files using LTP and TCPCL

## How to use
* Open the "sender" and "receiver" directories as per your respective container
* For the sender container, put all of the files you want to send over to the receiver within the fo
lder titled "flightdata". Once the test is over, a folder called "received" will be created on the r
eceiver side of the application
* Once you have put all the files you want to be transfered, you are now ready to start the receiver
 script
    * run ./start\_ltp\_receive_bpv6.sh for the ltp test using Bpv6, add the option "-c 1" to enable
 custody transfer
    * run ./start\_ltp\_receive_bpv7.sh for the ltp test using bpv7, add the option "-s 1" to enable
 security
    * run ./start\_tcpcl\_receive.sh for the tcpcl test
* Wait for about 3 seconds
* The start the corresponding sender script
    * run ./start\_ltp\_send_bpv6.sh for the ltp test using bpv6, add the option "-c 1" to enable cu
stody transfer
    * run ./start\_ltp\_send_bpv7.sh for the ltp test using bpv7, add the option "-s 1" to enable se
curity
    * run ./start\_tcpcl\_send.sh for the tcpcl test
* Wait about 10 seconds for the sender and receiver programs to initialize and operate
* You can then use tcpdump to observe the network traffic from:
    * the sender container
    * the receiver container
    * the host machine running the container
* Running tcpdump on these different locations will show a different perspective on network traffic 
which may be beneficial for your testing

