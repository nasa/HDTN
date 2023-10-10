import zmq, time, json, argparse

HDTN_SOCKET_ADDR="tcp://127.0.0.1:10305"

parser = argparse.ArgumentParser(description='Sets the LTP rate limit')
parser.add_argument('rateBitsPerSec', type=int, help='The bit-per-second rate to set')
args = parser.parse_args()

reqdata = {
    "apiCall": "set_max_send_rate",
    "rateBitsPerSec": args.rateBitsPerSec,
    "outduct": 0
}

context = zmq.Context()
socket = zmq.Socket(context, zmq.REQ)
socket.connect(HDTN_SOCKET_ADDR)
start = time.time()
socket.send_json(reqdata)
resp = socket.recv()
resp = json.dumps(json.loads(resp), indent=2)
print(resp)
print()
