#include "imtui/imtui.h"

#include "imtui/imtui-impl-ncurses.h"

#include <stdio.h>
#include <math.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <signal.h>

#define perror_exit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define serverPort 8080



/*
    Class for a connection to the server
    ! Dont run threads in this classs, it will make everything bad !
*/
class connection{
public: // Making everything public temporarily
    int sockFd = -1; // Socket file descriptor - assumed to be valid when the object is created
    struct sockaddr_in address; // Address of the client
    int addrlen; // Length of the address
    std::string username;
    std::string hostname;
    std::string publicIp; // Public IP of the client - sent by the client since we can't get it from the socket due to the usage of TOR
    std::mutex sockFdMutex; // Only one thread can use the socket file descriptor at a time
    bool lastConnectivityCheck = false; // Last result of the connectivity check

    /*
        Constructor
    */
   connection(int sockFd, struct sockaddr_in address, int addrlen){
       this->sockFd = sockFd;
       this->address = address;
       this->addrlen = addrlen;
    }

    ~connection(){
        this->sockFdMutex.lock();
        close(this->sockFd);
        this->sockFdMutex.unlock();
    }

    bool sanityCheck(){
        if(this->sockFd < 0){
            return false;
        }
        return true;
    }

    void drawDebugWindow(){
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(25, 6), ImGuiCond_Once);
        ImGui::Begin(("Socket " + std::to_string(this->sockFd) + " debug info").c_str());
        ImGui::Text("Username: %s", this->username.c_str());
        ImGui::Text("Hostname: %s", this->hostname.c_str());
        ImGui::Text("Public IP: %s", this->publicIp.c_str());
        ImGui::Text("Connectivity: %s", (this->lastConnectivityCheck)?"OK":"ERR");
        ImGui::End();
    }

    bool connectivityCheck(){
        // Send "ping" to the client, if the client does not respon with "ping;pong;", return false
        this->sockFdMutex.lock();
        int bytesSent = send(this->sockFd, "ping;", 5, 0);
        if(bytesSent != 5){
            this->sockFdMutex.unlock();
            this->lastConnectivityCheck = false;
            return false;
        }
        char buffer[10] = {0};
        int bytesReceived = recv(this->sockFd, buffer, 10, 0);
        if(std::string(buffer) != "ping;pong;"){
            this->sockFdMutex.unlock();
            this->lastConnectivityCheck = false;
            return false;
        }
        this->sockFdMutex.unlock();
        this->lastConnectivityCheck = true;
        return true;
    }

    bool intialConnection(){
        this->sockFdMutex.lock();
        // Receive the username, hostname, and public IP from the client
        // It must be supplied in the format "ip;username;hostname;"
        char buffer[1024] = {0}; // Should easily be enough space
        int bytesReceived = recv(this->sockFd, buffer, 1024, 0);
        if(bytesReceived < 0){
            return false;
        }
        std::string bufferString = std::string(buffer); // Convert to a string for easier parsing
        if(std::count(bufferString.begin(), bufferString.end(), ';') != 3){
            return false;
        }
        this->publicIp = bufferString.substr(0, bufferString.find(';'));
        bufferString = bufferString.substr(bufferString.find(';') + 1);
        this->username = bufferString.substr(0, bufferString.find(';'));
        bufferString = bufferString.substr(bufferString.find(';') + 1);
        this->hostname = bufferString.substr(0, bufferString.find(';'));
        // Send the connection confirmation message to the client
        // <ip>;<username>;<hostname>;connected;
        std::string confirmMsg = this->publicIp + ";" + this->username + ";" + this->hostname + ";connected;";
        int bytesSent = send(this->sockFd, confirmMsg.c_str(), confirmMsg.length(), 0);
        this->sockFdMutex.unlock();
        return true; // TODO: Properly verify the data received is valid
    }
};

class server{
public: // Also making everything public temporarily
    int serverListenerFd; // Socket file descriptor for the server to listen for new connections on
    struct sockaddr_in address; // Address of the server
    int opt = 1; // Used for setsockopt
    std::vector<std::unique_ptr<connection>> connections; // Vector of connections to the server
    std::mutex connectionsMutex; // Mutex for the connections vector
    unsigned int maxConnections; // Maximum number of connections to the server
    int selectedConnection = 0; // Index of the selected connection in the connections vector
    bool DEBUG = false;
    unsigned long int n_conn = 0; // For debugging purposes

    /*
        Constructor - Sets up a socket to listen for new connections on and starts the listener thread
    */
    server(unsigned int port = 8080, unsigned int maxConnections = 32){
        this->maxConnections = maxConnections;
        // Creating the socket file descriptor
        if ((this->serverListenerFd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror_exit("server::server() - ERR: socket() failed");
        }
        
        // Forcefully attaching socket to the port 8080
        if (setsockopt(this->serverListenerFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &this->opt, sizeof(this->opt))) {
            perror_exit("server::server() - ERR: setsockopt() failed");
        }
        this->address.sin_family = AF_INET;
        this->address.sin_addr.s_addr = INADDR_ANY;
        this->address.sin_port = htons(port);

        // Forcefully attaching socket to the port 8080
        if (bind(this->serverListenerFd, (struct sockaddr *)&this->address, sizeof(this->address))<0) {
            perror_exit("server::server() - ERR: bind() failed");
        }
        if (listen(this->serverListenerFd, 3) < 0) { // 3 is the maximum number of pending connections, any more and the client will get a "Connection refused" error
            perror_exit("server::server() - ERR: listen() failed");
        }

        // Starting the listener thread
        std::thread listenerThread(&server::listenerThreadFunction, this);
        listenerThread.detach(); // Detaching the thread so it doesn't need to be joined

        // Starting the connectivity checking thread
        std::thread connectivityCheckThread(&server::connectivityCheckThreadFunction, this);
        connectivityCheckThread.detach();
    }

    void draw(){
        if(this->DEBUG){
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(30, 4), ImGuiCond_Once);
            ImGui::Begin("Server Debug Window");
            ImGui::Text("Number of active connections: %d", this->connections.size());
            ImGui::Text("Number of connections since start: %d", this->n_conn);
            ImGui::End();
            this->connectionsMutex.lock();
            for(auto &c : this->connections){
                c->drawDebugWindow();
            }
            this->connectionsMutex.unlock();
        }

        // Draw the list of connections
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(30,ImGui::GetIO().DisplaySize.y/2), ImGuiCond_Once);
        ImGui::Begin("Server Connections");
        // Have to make a vector of strings to pass to the ListBox function
        // Because it requires a pointer to the stuff to write onto the screen >:(
        std::vector<std::string> connInfoStrings;
        this->connectionsMutex.lock();
        for(auto &c : this->connections){
            connInfoStrings.push_back(c->username + "@" + c->hostname + " (" + c->publicIp + ")");
        }
        this->connectionsMutex.unlock();
        if(connInfoStrings.size() == 0){
            connInfoStrings.push_back("No connections");
        }
        ImGui::PushItemWidth(-1);
        ImGui::ListBox("##Connections", 
                       &this->selectedConnection, 
                       [](void* data, int idx, const char** out_text){
                            std::vector<std::string>* connInfoStrings = (std::vector<std::string>*)data;
                            *out_text = connInfoStrings->at(idx).c_str();
                            return true;
                       },
                       (void*)&connInfoStrings,
                       connInfoStrings.size(),
                       connInfoStrings.size()
                       );
        ImGui::PopItemWidth();
        ImGui::End();
    }

    /*
        Function that the listener thread runs
        Places new connections into the connections vector, does nothing else.
        (Hopefully) thread safe via the connectionsMutex
    */
    void listenerThreadFunction(){
        while(true){
            this->connectionsMutex.unlock();
            // Dont allow more than 10 connections:
            if(this->connections.size() >= this->maxConnections){
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            // Set up the variables required to accept a new connection
            int newConnectionFd;
            struct sockaddr_in address;
            int addrlen = sizeof(address);
            // Wait for a new connection
            if ((newConnectionFd = accept(this->serverListenerFd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror_exit("server::listenerThreadFunction() - ERR: accept() failed");
            }
            this->n_conn++;
            // Create a unique_ptr to a new connection object for the new connection
            std::unique_ptr<connection> newConnection = std::make_unique<connection>(newConnectionFd, address, addrlen);
            // Verify the connection using the intialConnection() function (part of the connection class)
            if(!newConnection->intialConnection()){
                continue; // If the connection failed, dont add it to the connections vector
            }
            this->connectionsMutex.lock();
            // Move ownership of the connection to the connections vector
            this->connections.push_back(std::move(newConnection));
            this->connectionsMutex.unlock();
        }
    }

    /*
        Function that the connectivity checking thread runs
        Checks if the connections are still connected by sending a ping to them
        (Hopefully) thread safe via the connectionsMutex :D
    */
    void connectivityCheckThreadFunction(){
        while(true){
            std::this_thread::sleep_for(std::chrono::seconds(1));
            this->connectionsMutex.lock();
            // Iterate over every connection, delete it from the vector if connectivityCheck() returns false
            for(int i = 0; i < this->connections.size(); i++){
                if(this->connections[i] == NULL){ // Might not actually fix anything
                    this->connections.erase(this->connections.begin() + i);
                    i--;
                    continue;
                }
                if(!this->connections[i]->connectivityCheck()){
                    this->connections.erase(this->connections.begin() + i);
                    i--;
                }
            }
            this->connectionsMutex.unlock();
        }
    }

};



int main() {
    // Make all threads ignore SIGPIPE
    // This is so that if a connection is closed, the program doesn't crash
    signal(SIGPIPE, SIG_IGN);
    sigset_t sigpipeMask;
    sigemptyset(&sigpipeMask);
    sigaddset(&sigpipeMask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &sigpipeMask, NULL);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();

    server serverInstance(8080, 16);
    serverInstance.DEBUG = true;
    
    while (true) {
        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();
        ImGui::NewFrame();

        serverInstance.draw();

        // Update and render
        ImGui::Render();
        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();
    }

    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();

    return 0;
}