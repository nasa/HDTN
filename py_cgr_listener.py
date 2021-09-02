from pycgr.py_cgr.py_cgr_lib.py_cgr_lib import *
import zmq
import time
import sys
import random
import json
import re

port = "5556"
context = zmq.Context()
socket = context.socket(zmq.PULL)
socket.bind("tcp://127.0.0.1:%s" % port) #localhost caused error
contact_plan = cp_load('cgrTable.json', 5000)

curr_time = 0

while True:
    msg = socket.recv()
    print("message received by server")
    splitMessage = re.split('|', msg)
    sourceId = splitMessage[0]
    destinationId = splitMessage[1]
    startTime = splitMessage[2]
    root_contact = Contact(sourceId, sourceId, 0, sys.maxsize, 100, 1, 0)


    root_contact.arrival_time = startTime

    route = cgr_dijkstra(root_contact, destinationId, contact_plan)

    route.hops[1] #next hop shouldn be 2nd on route list
    print(route)
    print("sending next hop: " + route.hops[1])
    socket.send(route.hops[1])
    time.sleep(1)


