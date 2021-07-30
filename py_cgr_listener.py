from pycgr.py_cgr.py_cgr_lib.py_cgr_lib import *
import zmq
import time
import sys
import random
import json

port = "5556"
context = zmq.Context()
socket = context.socket(zmq.PAIR)
socket.bind("tcp://*:%s" % port)
contact_plan = cp_load(cp_file)

curr_time = 0

while True:
    msg = socket.recv()
    socket.send("message received by client")
    root_contact = Contact(msg.sourceId, msg.sourceId, 0, sys.maxsize, 100, 1, 0)
    root_contact.arrival_time = curr_time #Does this need to be iterated

    route = cgr_dijkstra(root_contact, msg.destinationId, contact_plan)

    route.hops[1] #next hop shouldn be 2nd on route list
    print(route)
    print("sending next hop: " + route.hops[1])
    socket.send(route.hops[1])
    time.sleep(1)


