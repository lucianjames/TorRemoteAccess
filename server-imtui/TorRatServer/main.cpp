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

#define perror_exit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

/*
    The connections struct is used by the connections manager to store information about each connection.
*/
struct connection{
    int fd;
    std::string publicIp;
    std::string username;
    std::string hostname;
    bool inUse = false;
    connection(int fd, std::string publicIp, std::string username, std::string hostname){
        this->fd = fd;
        this->publicIp = publicIp;
        this->username = username;
        this->hostname = hostname;
    }
};

/*
    The connections manager class is responsible for keeping track of active connections and displaying them in a listbox.
    It also handles ping/pong to check if a connection is still active. (this happens in a separate thread)
*/
class connectionsManager{
private:
public: // Everything public for testing and hacking
    std::vector<connection> connections = {}; // Vector of connections
    std::mutex connectionsMutex = {}; // Mutex to lock the connections vector
    std::thread pingThread; // Thread to check if connections are still active
    
    void pingConnections(){ // Runs in a separate thread
        while (true){
            for (int i = 0; i < this->connections.size(); i++){
                if (this->connections[i].inUse){
                    continue; // Skip this connection
                }
                this->connections[i].inUse = true; // Set the inUse flag to true to prevent other threads from accessing this connection
                send(this->connections[i].fd, "ping;", 5, 0); // We expect a response of "ping;pong;"" from the client
                char buffer[11] = {0};
                int valread = recv(this->connections[i].fd, buffer, 11, 0);
                if (valread == 0 || std::string(buffer) != "ping;pong;"){
                    // If the client fails to respond to the ping, or the response is invalid, remove the connection from the connections vector
                    this->connectionsMutex.lock(); // Lock the connections vector to ensure no other threads are accessing it at the same time
                    close(this->connections[i].fd); // Close the socket
                    this->connections.erase(this->connections.begin() + i); // Remove the connection from the connections vector
                    this->connectionsMutex.unlock(); // Unlock the connections vector
                }else{
                    this->connections[i].inUse = false; // Set the inUse flag to false to allow other threads to access this connection
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Sleep for 2 seconds
        }
    }
    
    connectionsManager(){
        this->pingThread = std::thread(&connectionsManager::pingConnections, this);
        this->pingThread.detach();
    }

    ~connectionsManager(){
        for (auto c : this->connections){
            close(c.fd);
        }
    }

    void newConnectionThread(int fd){
        /*
        Connection message format:
            <ip>;<username>;<hostname>;
        */
        char buffer[1024] = {0};
        int bytesRead = recv(fd, buffer, 1024, 0);
        if(bytesRead == 0){
            // If the client has disconnected, return
            return;
        }
        std::string connectionMessage = buffer; // Convert to string for easier parsing
        if(std::count(connectionMessage.begin(), connectionMessage.end(), ';') != 3){
            // If the message is invalid, return
            return;
        }
        std::string publicIp = connectionMessage.substr(0, connectionMessage.find(';'));
        connectionMessage.erase(0, connectionMessage.find(';') + 1);
        std::string username = connectionMessage.substr(0, connectionMessage.find(';'));
        connectionMessage.erase(0, connectionMessage.find(';') + 1);
        std::string hostname = connectionMessage.substr(0, connectionMessage.find(';'));
        connectionMessage.erase(0, connectionMessage.find(';') + 1);
        // Send back a message to the client to confirm the connection
        /*
            Format:
                <ip>;<username>;<hostname>;connected;
        */
        std::string response = publicIp + ";" + username + ";" + hostname + ";connected;";
        send(fd, response.c_str(), response.length(), 0);
        // Add the connection to the connections vector
        connection newConnection(fd, publicIp, username, hostname);
        this->connectionsMutex.lock();
        this->connections.push_back(newConnection);
        this->connectionsMutex.unlock();
    }

    void newConnection(int fd){
        // Spawn a thread to handle initialising communication with the client
        // This includes getting the initial connection information and adding the connection to the connections vector
        std::thread newConnThread(&connectionsManager::newConnectionThread, this, fd);
        newConnThread.detach();
    }


};

/*
    The server class is responsible for starting the server and listening for incoming connections.
    It also contains a connections manager object to keep track of active connections.
*/

class server{
private:
public: // Everything public for testing and hacking
    int fd; // File descriptor for the socket
    unsigned int port = 8080; // Port to listen on
    struct sockaddr_in serverAddress; // Server address
    connectionsManager connManager; // Connections manager object
    std::thread listenAcceptThread; // Thread to accept incoming connections

    void acceptConnections(){
        while (true){
            int newSocket;
            struct sockaddr_in clientAddress;
            int addrlen = sizeof(clientAddress);
            if ((newSocket = accept(this->fd, (struct sockaddr *)&clientAddress, (socklen_t*)&addrlen)) < 0){
                perror_exit("server::acceptConnections() -> accept(): ERR: Failed to accept incoming connection");
            }
            // Pass the new socket to the connections manager, which will handle initialising communication with the client and adding it to the connections vector
            this->connManager.newConnection(newSocket);
        }
    }

    void startListener(){
        this->fd = socket(AF_INET, SOCK_STREAM, 0);
        if (this->fd == -1){
            perror_exit("server::startListener() -> socket(): ERR: Failed to create socket");
        }
        int opt = 1;
        if (setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
            perror_exit("server::startListener() -> setsockopt(): ERR: Failed to set socket options");
        }
        this->serverAddress.sin_family = AF_INET;
        this->serverAddress.sin_addr.s_addr = INADDR_ANY;
        this->serverAddress.sin_port = htons(this->port);
        if (bind(this->fd, (struct sockaddr *)&this->serverAddress, sizeof(this->serverAddress)) < 0){
            perror_exit("server::startListener() -> bind(): ERR: Failed to bind socket");
        }
        if (listen(this->fd, 3) < 0){
            perror_exit("server::startListener() -> listen(): ERR: Failed to listen on socket");
        }
        // Spawn a thread to accept incoming connections
        this->listenAcceptThread = std::thread(&server::acceptConnections, this);
        this->listenAcceptThread.detach();
    }
    server(unsigned int port=8080){
        this->port = port;
        this->startListener();
    }
    ~server(){
        close(this->fd);
    }
};


int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();

    int nframes = 0;
    float fval = 1.23f;

    server serverInstance(8080);

    /*
    while(1){
        std::this_thread::sleep_for(std::chrono::seconds(5));
        // Print the number of connections
        printf("Number of connections: %d\n", serverInstance.connManager.connections.size());
        printf("Connections vector contents:\n");
        for (auto c : serverInstance.connManager.connections){
            printf("fd: %d, publicIp: %s, username: %s, hostname: %s\n", c.fd, c.publicIp.c_str(), c.username.c_str(), c.hostname.c_str());
        }
        printf("====================================\n\n");
    }
    */


    int connectionsListSelected = 0;
    while (true) {
        // All the stuff that isnt directy calling ImGui
        std::vector<std::string> connectionInfo;
        for(auto c : serverInstance.connManager.connections){
            connectionInfo.push_back(c.publicIp + ";" + c.username + ";" + c.hostname);
        }
        if(connectionInfo.size() == 0){
            connectionInfo.push_back("No active connections....");
        }

        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();
        ImGui::NewFrame();


        ImGui::SetNextWindowPos(ImVec2(4, 27), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(50.0, 10.0), ImGuiCond_Once);
        ImGui::Begin("Hello, world!");
        ImGui::Text("NFrames = %d", nframes++);
        ImGui::Text("Mouse Pos : x = %g, y = %g", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
        ImGui::Text("Time per frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("Float:");
        ImGui::SameLine();
        ImGui::SliderFloat("##float", &fval, 0.0f, 10.0f);
        ImGui::End();


        /*
            Connections list window
        */
        ImGui::SetNextWindowPos(ImVec2(4, 4), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(50.0, 20.0), ImGuiCond_Once);
        ImGui::Begin("Connections");
        ImGui::PushItemWidth(-1);
        ImGui::ListBox("##connections", 
                       &connectionsListSelected, 
                       [](void* data, int idx, const char** out_text){ auto& connections = *static_cast<std::vector<std::string>*>(data); *out_text = connections[idx].c_str();  return true;},
                       static_cast<void*>(&connectionInfo), connectionInfo.size(), connectionInfo.size());
        /*
            The messy lambda function above gets the connection info from the connections vector at the index idx, and sets the out_text pointer to the c_str() of the connection info string
            at that index. The return value is true, which means that the listbox will continue to iterate through the connections vector until it reaches the end.
        */
        ImGui::PopItemWidth();
        ImGui::End();


        ImGui::Render();
        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();
    }

    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();

    return 0;
}