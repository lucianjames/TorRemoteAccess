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

# Close the connection
sock.close()
