import socket
import random

# Send the following string to the server:
# <ip>;<username>;<hostname>;

# Fill each out with something appropriate

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 52727))

# Generate a random username
username = ""
for i in range(0, 10):
    username += chr(random.randint(65, 90))

sock.send("127.0.0.1;{};{};".format(username, socket.gethostname()).encode("utf-8"))

# Wait for response
data = sock.recv(1024)
print(data)

while(1):
    # Receive data from the server, if it is "ping;" then send back "ping;pong;"
    data = sock.recv(1024)
    if data.decode("utf-8") == "ping;":
        print("sending pong")
        sock.send("ping;pong;".encode("utf-8"))
        continue
    if data.decode("utf-8") == "":
        break
    print(data)
    sock.send(data)

# Close the connection
sock.close()
