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
    unsigned int connectivityCheckIntervalSeconds = 5; // Interval between connectivity checks

    /*
        Constructor - Sets up a socket to listen for new connections on and starts the listener thread
    */
    server(unsigned int port = 8080, unsigned int maxConnections = 32){
        this->maxConnections = maxConnections;
        // Creating the socket file descriptor
        if ((this->serverListenerFd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror_exit("server::server() - ERR: socket() failed");
        }
        
        // Set up the socket options
        if (setsockopt(this->serverListenerFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &this->opt, sizeof(this->opt))) {
            perror_exit("server::server() - ERR: setsockopt() failed");
        }
        this->address.sin_family = AF_INET;
        this->address.sin_addr.s_addr = INADDR_ANY;
        this->address.sin_port = htons(port);

        // Bind the socket to the address we just set up
        if (bind(this->serverListenerFd, (struct sockaddr *)&this->address, sizeof(this->address))<0) {
            perror_exit("server::server() - ERR: bind() failed");
        }

        // Make the socket listen for connectoins
        if (listen(this->serverListenerFd, 3) < 0) { // 3 is the maximum number of pending connections, any more and the client will get a "Connection refused" error
            perror_exit("server::server() - ERR: listen() failed");
        }

        // Start up the thread that listens for and sets up connections
        std::thread listenerThread(&server::listenerThreadFunction, this);
        listenerThread.detach(); // Detaching the thread so it doesn't need to be joined

        // Start the thread which loops over every established connection (which doesnt have an active terminal) and checks if its still alive
        std::thread connectivityCheckThread(&server::connectivityCheckThreadFunction, this);
        connectivityCheckThread.detach();
    }

    void draw(){
        if(this->DEBUG){ // If debugging is enabled, draw a window which shows a little bit of info about the server
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(30, 4), ImGuiCond_Once);
            ImGui::Begin("Server Debug Window");
            ImGui::Text("Number of active connections: %d", this->connections.size());
            ImGui::Text("Number of connections since start: %d", this->n_conn);
            ImGui::End();
            // Also, draw the debugging windows built into each connection:
            this->connectionsMutex.lock(); // Lock the connections vector so no other thread can modify while debug windows are being drawn
            for(auto &c : this->connections){
                c->drawDebugWindow(); // Draw each windows debug info window
            }
            this->connectionsMutex.unlock();
        }

        // Draw the list of connections window
        unsigned int ConnListWindowWidth = 100;
        unsigned int ConnListWindowHeight = 10;
        ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x/2)-(ConnListWindowWidth/2), 0), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(ConnListWindowWidth, ConnListWindowHeight), ImGuiCond_Once);
        ImGui::Begin("Server Connections");
        // Have to make a vector of strings to pass to the ListBox function
        // Because it requires a pointer to the stuff to write onto the screen >:(
        std::vector<std::string> connInfoStrings; // Creating this every single frame is not very efficient, but its not really a problem
        this->connectionsMutex.lock(); // Make sure the connections arent being modified while the connInfoStrings are being assembled
        for(auto &c : this->connections){
            connInfoStrings.push_back(c->username + "@" + c->hostname + " (" + c->publicIp + ") on socket " + std::to_string(c->sockFd));
        }
        this->connectionsMutex.unlock();
        if(connInfoStrings.size() == 0){
            connInfoStrings.push_back("No connections"); // This prevents the listbox from looking weird when there are no connections
        }
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        ImGui::ListBox("##Connections", 
                       &this->selectedConnection, 
                       [](void* data, int idx, const char** out_text){
                            std::vector<std::string>* connInfoStrings = (std::vector<std::string>*)data; // Cast the void* to an std::vector<std::string>*
                            *out_text = connInfoStrings->at(idx).c_str(); // Get a pointer to the string at the current index (idx)
                            return true;
                       },
                       (void*)&connInfoStrings, // This is the void* data variable of the lambda function
                       connInfoStrings.size(),
                       connInfoStrings.size()
        );
        ImGui::End();
    }

    void update(){ // Called every frame
        this->connectionsMutex.lock(); // Lock the connections vector so no other thread can modify it while we are updating
        for(int i=0; i<this->connections.size(); i++){ // For every connection.....
            if(this->connections[i]->terminalActive && this->selectedConnection != i){ // If a connection is active, but it isnt the currently selected terminal
                this->connections[i]->closeTerminal(); // Do some cleanup, because its about to be moved back to the background
            }
            this->connections[i]->terminalActive = (i==this->selectedConnection)?true:false; // Set the terminalActive variable to true if the connection is currently selected in the UI
            this->connections[i]->update(); // Update the connection
        }
        this->connectionsMutex.unlock(); // Other threads can have fun again :)
    }

    /*
        Function that the listener thread runs
        Places new connections into the connections vector, does nothing else.
        (Hopefully) thread safe via the connectionsMutex
    */
    void listenerThreadFunction(){
        while(true){ // This thread goes until the program is closed
            // Dont allow more than 10 connections:
            if(this->connections.size() >= this->maxConnections){
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep for a bit to reduce CPU usage, helps prevent DOS kinda stuff from happening :)
                continue; // Its been a whole 100ms, so lets go back to the top of the loop
            }
            // Set up the variables required to accept a new connection
            int newConnectionFd; // The file descriptor for the new connection, will be set by accept() then used to instantiate a new connection object
            struct sockaddr_in address;
            int addrlen = sizeof(address);
            // Wait for a new connection
            if ((newConnectionFd = accept(this->serverListenerFd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror_exit("server::listenerThreadFunction() - ERR: accept() failed");
            }
            this->n_conn++; // Increment the variable used to keep track of the number of connections
            // Create a unique_ptr to a new connection object for the new connection
            std::unique_ptr<connection> newConnection = std::make_unique<connection>(newConnectionFd, address, addrlen);
            // Verify the connection using the intialConnection() function (part of the connection class)
            if(!newConnection->intialConnection()){
                continue; // If the connection failed, dont add it to the connections vector
            }
            this->connectionsMutex.lock(); // Make sure this thread is the only one using the connections vector
            this->connections.push_back(std::move(newConnection)); // Move ownership of the unique_ptr<connection> to the connections vector
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
            std::this_thread::sleep_for(std::chrono::seconds(connectivityCheckIntervalSeconds));
            this->connectionsMutex.lock();
            // Iterate over every connection, delete it from the vector if connectivityCheck() returns false
            for(int i = 0; i < this->connections.size(); i++){
                // Under extreme stress-test conditions, the connections vector can contain NULL pointers (not sure how)
                if(this->connections[i] == NULL){ // This is a hack to prevent a segfault
                    this->connections.erase(this->connections.begin() + i);
                    i--;
                    continue;
                }
                if(this->connections[i]->terminalActive){ // Dont send a ping to the connection if its terminal is active
                    continue;
                }
                if(!this->connections[i]->connectivityCheck()){ // If the connectivity check fails, delete the connection
                    this->connections.erase(this->connections.begin() + i);
                    if(this->selectedConnection > i){ // So we dont mess up whats currently selected in the UI
                        this->selectedConnection--;
                    }
                    i--;
                }
            }
            this->connectionsMutex.unlock();
        }
    }

};