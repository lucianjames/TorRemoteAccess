#include <sys/socket.h>
#include <arpa/inet.h>

// Check that TOR is running by connecting to 127.0.0.1:9050
bool torRunning(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(9050);
    return connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != -1;
}
