import base64
import json
import logging
import zmq
from hdtn.registration import RegistrationManager

HDTN_REG_PORT = 10140

if __name__ == '__main__':
    registry = RegistrationManager()
    logging.basicConfig(format='[%(asctime)s] %(message)s', level=logging.INFO)
    logging.info("Registration server: starting up ...")
    port = HDTN_REG_PORT

    ctx = zmq.Context()
    target = ctx.socket(zmq.REP) #pylint: disable=E1101
    target.bind("tcp://*:%s" % port)

    try:
        while True:
            message = target.recv(copy=False)
            
            # JCF -- Added a way to programmatically stop service
            if (str(message) == "SHUTDOWN"):
            	logging.info("Exiting on user command (programmatically) ...")
            	break;
            	
            res = registry.dispatch(message)
            if res is not None:
                target.send_string("HDTN/1.0 200 OK | " + json.dumps({"hdtnEntryList" : res}))
            else:
                target.send_string("HDTN/1.0 400 FAIL | ")
    except KeyboardInterrupt:
        logging.info("Exiting on user request (keyboard interrupt) ...")
