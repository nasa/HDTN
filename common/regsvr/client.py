import zmq

if __name__ == '__main__':
    context = zmq.Context()
    socket = context.socket(zmq.REQ)                           #pylint: disable=E1101
    socket.set_string(zmq.IDENTITY, "storage:10141:PUSH")      #pylint: disable=E1101
    socket.connect("tcp://localhost:10140")
    socket.send_string("HDTN/1.0 REGISTER")
    print("Registration: " + socket.recv().decode('utf-8'))
    socket.send_string("HDTN/1.0 QUERY")
    print("Query all: " + socket.recv().decode('utf-8'))
    socket.send_string("HDTN/1.0 QUERY storage")
    print("Query storage: " + socket.recv().decode('utf-8'))
    socket.send_string("HDTN/1.0 QUERY noexist")
    print("Query noexist: " + socket.recv().decode('utf-8'))
    socket.send_string("HDTN/1.0 DEREGISTER")
    print("Deregistration: " + socket.recv().decode('utf-8'))
    socket.send_string("HDTN/1.0 QUERY")
    print("Query all: " + socket.recv().decode('utf-8'))
    socket.close()
