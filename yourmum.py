import socket
import random

# Send the following string to the server:
# <ip>;<username>;<hostname>;

# Fill each out with something appropriate


socks = []
for i in range (0, 1000):
    socks.append(socket.socket(socket.AF_INET, socket.SOCK_STREAM))
    socks[i].connect(("127.0.0.1", 8080))
    print("Connected to server {}".format(i))
    socks[i].send("127.0.0.1;{};{};".format("sussyballs", socket.gethostname()).encode("utf-8"))
    print("Sent data to server {}".format(i))
    d = socks[i].recv(1024)
    print("Received data from server {}: {}".format(i, d))

    

# Close all the connections
for s in socks:
    s.close()
