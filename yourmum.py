import socket
import random
from time import sleep

while(1):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("127.0.0.1", 8080))
    sock.send("127.0.0.1;{};{};".format("sussyballs", socket.gethostname()).encode("utf-8"))
    data = sock.recv(1024)
    sock.send(data)
    sock.close()
