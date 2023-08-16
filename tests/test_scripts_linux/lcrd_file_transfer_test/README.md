# File Transfer Test
* Tests the ability to send a file using HDTN
* This test supports sending files using LTP and TCPCL

## How to use
* Open the "sender" and "receiver" directories as per your respective container
* For the sender container, make a folder titled "flightdata" and put all of the files you want to send over to the receiver within this folder. Once the test is over, a folder called "received" will be created on the receiver side of the application
* Once you have put all the files you want to be transfered, you are now ready to start the receiver script
    * run ./start\_ltp\_receive.sh for the ltp test
    * run ./start\_tcpcl\_receive.sh for the tcpcl test
* Wait for about 3 seconds
* The start the corrisponding sender script
    * run ./start\_ltp\_send.sh for the ltp test
    * run ./start\_tcpcl\_send.sh for the tcpcl test
* Wait about 10 seconds for the sender and receiver programs to initialize and operate
* You can then use tcpdump to observe the network traffic from:
    * the sender container
    * the receiver container
    * the host machine running the container
* Running tcpdump on these different locations will show a different perspective on network traffic which many beneficial for your testing
