import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

server_address = '0.0.0.0'
server_port = 4560
# server_port = 7132
msg = "hello world"

sent = sock.sendto(bytes(msg, "utf-8"), (server_address, server_port))
if (sent):
    print("Sent message \"" + msg + "\" to " + server_address + ":" + str(server_port))
else:
    print("Error sending message")
