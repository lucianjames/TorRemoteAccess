import socket


# Send the following string to the server:
# <ip>;<username>;<hostname>;

# Fill each out with something appropriate

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 8080))

sock.send("127.69.69.69;bruh moment;computah;".encode("utf-8"))

# Wait for response
data = sock.recv(1024)
print(data)

while(1):
    # Receive data from the server, if it is "ping;" then send back "ping;pong;"
    data = sock.recv(1024)
    if data.decode("utf-8") == "ping;":
        print("sending pong")
        sock.send("ping;pong;".encode("utf-8"))
    if data.decode("utf-8") == "":
        break
    print(data)

# Close the connection
sock.close()
