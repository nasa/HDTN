from pycgr.py_cgr.py_cgr_lib.py_cgr_lib import *
import zmq
import time
import sys
import random

port = "5556"
context = zmq.Context()
socket = context.socket(zmq.PAIR)
socket.bind("tcp://*:%s" % port)


while True:
    msg = socket.recv()
    socket.send("message received by client")
    route = cgr_dijkstra(msg.root_contact, msg.destination, msg.contact_plan)
    time.sleep(1)
