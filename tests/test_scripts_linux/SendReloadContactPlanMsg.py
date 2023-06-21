import zmq
import time
import sys
import random
import json
import re
import getopt
import struct 

context = zmq.Context()
socket = context.socket(zmq.REQ)
port = 10305
socket.connect("tcp://127.0.0.1:%s" % port) 

payload = "../../module/router/contact_plans/contactPlanCutThroughMode_unlimitedRate.json"

#open text file in read mode
text_file = open(payload, "r")

#read whole file to a string
data = text_file.read()

command = {"apiCall": "upload_contact_plan", "contactPlanJson": json.dumps(data)}

#close file
text_file.close()

print(data)

socket.send_json(command)

socket.recv()
