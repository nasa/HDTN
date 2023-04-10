import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

server_address = '0.0.0.0'
server_port = 7132

server = (server_address, server_port)
sock.bind(server)
print("Listening on " + server_address + ":" + str(server_port))

while True:
    payload, client_address = sock.recvfrom(1024)
    # print("Received payload: ",  payload.decode("hex"))
    print("Received payload: ",  str(payload))
    # print("Echoing data back to " + str(client_address) + ": " + payload.decode("ascii"))
    # sent = sock.sendto(payload, client_address)
