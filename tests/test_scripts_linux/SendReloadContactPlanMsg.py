import zmq
import time
import sys
import random
import json
import re
import getopt
import struct 

context = zmq.Context()
socket = context.socket(zmq.PUB)
port = "29001"
socket.bind("tcp://127.0.0.1:%s" % port) 

CPM_NEW_CONTACT_PLAN = int("FC09", 16)

command = CPM_NEW_CONTACT_PLAN 

payload = "../../module/scheduler/src/contactPlanCutThroughMode.json"

packed = struct.pack('<HHBBBB', command, 0, 0, 0, 0, 0)

print(packed)
time.sleep(10)
socket.send(packed, flags=zmq.SNDMORE)

#open text file in read mode
text_file = open(payload, "r")

#read whole file to a string
data = text_file.read()

#close file
text_file.close()

print(data)

time.sleep(10)
socket.send_string(data)

