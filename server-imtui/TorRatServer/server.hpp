#include "includeCrap.hpp"
#include "connection.hpp"

/*
    The server class manages new connections and checking the status of existing connections
*/
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

    void update(){
        this->connectionsMutex.lock();
        for(int i=0; i<this->connections.size(); i++){
            if(i != this->selectedConnection && this->connections[i]->terminalActive){
                this->connections[i]->closeTerminal();
            }
            this->connections[i]->terminalActive = (i==this->selectedConnection)?true:false;
            this->connections[i]->update();
        }
        this->connectionsMutex.unlock();
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
                if(this->connections[i]->terminalActive){
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