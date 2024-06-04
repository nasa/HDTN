#!/usr/bin/env python3
import zmq
import time

HDTN_SOCKET_ADDR="tcp://127.0.0.1:10305"

def main():
    context = zmq.Context()
    sock = zmq.Socket(context, zmq.REQ)

    sock.connect(HDTN_SOCKET_ADDR)

    with open("contact-plan.json", "r") as f:
        plan = f.read()

    reqdata = {
        "apiCall": "upload_contact_plan",
        "contactPlanJson": plan
    }

    sock.send_json(reqdata)
    resp = sock.recv()
    print(resp)
    sock.close()

if __name__ == "__main__":
    main()
