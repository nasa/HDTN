from pycgr.py_cgr.py_cgr_lib.py_cgr_lib import *
import zmq
import time
import sys
import random
import json
import re

port = "5556"
context = zmq.Context()
socket = context.socket(zmq.PAIR)
socket.bind("tcp://127.0.0.1:%s" % port) #localhost caused error
contact_plan = cp_load('cgrTable.json', 5000)

curr_time = 0

while True:
    msg = socket.recv()
    print("message received by server")
    print(msg)
    splitMessage = re.findall('[0-9]+', msg.decode('utf-8'))
    print(splitMessage)
    splitMessage = list(filter(None, splitMessage))
    print(splitMessage)    
    sourceId = int(splitMessage[0])
    destinationId = int(splitMessage[1])
    startTime = int(splitMessage[2])
    print(sourceId)
    print(destinationId)
    print(startTime)
    root_contact = Contact(sourceId, sourceId, 0, sys.maxsize, 100, 1, 0)
    

    root_contact.arrival_time = startTime
    print(root_contact)
    route = cgr_dijkstra(root_contact, destinationId, contact_plan)
    print("Here's the route")
    print(route)
    print("sending next hop: " + str(route.next_node))
    socket.send_string(str(route.next_node))
    time.sleep(1)


