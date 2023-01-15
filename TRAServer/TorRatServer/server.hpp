#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>

#include "connection.hpp"
#include "logWindow.hpp"

#define perror_exit(msg) perror(msg); exit(EXIT_FAILURE);

/*
    The server class manages new connections and checking the status of existing connections
*/
class server{
private:
    int serverListenerFd; // Socket file descriptor for the server to listen for new connections on
    struct sockaddr_in address; // Address of the server
    int opt = 1; // Used for setsockopt
    std::vector<std::unique_ptr<connection>> connections; // Vector of connections to the server
    std::mutex connectionsMutex; // Mutex for the connections vector
    unsigned int maxConnections; // Maximum number of connections to the server
    int selectedConnection = 0; // Index of the selected connection in the connections vector
    unsigned long int n_conn = 0; // For debugging purposes
    int connectivityCheckIntervalSeconds = 10; // Interval between connectivity checks
    logWindow servLog; // Log window for the server
    bool debugEnabled = false;
    bool checkConnectivity = true; // If true, the server will check the connectivity of all connections every connectivityCheckIntervalSeconds seconds

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
            this->servLog.add("server::listenerThreadFunction() - INFO: Waiting for new connection");
            if ((newConnectionFd = accept(this->serverListenerFd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror_exit("server::listenerThreadFunction() - ERR: accept() failed");
            }
            this->servLog.add("server::listenerThreadFunction() - INFO: New connection accepted");
            this->n_conn++; // Increment the variable used to keep track of the number of connections
            // Create a unique_ptr to a new connection object for the new connection
            std::unique_ptr<connection> newConnection = std::make_unique<connection>(newConnectionFd, &this->servLog);
            // Verify the connection using the intialConnection() function (part of the connection class)
            this->servLog.add("server::listenerThreadFunction() - INFO: Verifying connection");
            if(!newConnection->intialConnection()){
                this->servLog.add("server::listenerThreadFunction() - WARN: Connection failed verification, discarding");
                continue; // If the connection failed, dont add it to the connections vector
            }
            this->servLog.add("server::listenerThreadFunction() - INFO: Connection verified - adding to the list of connections");
            this->connectionsMutex.lock(); // Make sure this thread is the only one using the connections vector
            this->connections.push_back(std::move(newConnection)); // Move ownership of the unique_ptr<connection> to the connections vector
            this->connectionsMutex.unlock();
        }
    }

    /*
        Used to check connectivity just once, runs in the main thread
        Does not ignore active terminals
    */
    void connectivityCheckOnce(){
        this->servLog.add("server::connectivityCheckOnce() - INFO: Checking connections...");
        this->connectionsMutex.lock();
        for(int i = 0; i < this->connections.size(); i++){
            this->servLog.add("server::connectivityCheckOnce() - INFO: Checking connection " + std::to_string(i));
            if(this->connections[i] == NULL){ // This is a hack to prevent a segfault
                this->servLog.add("server::connectivityCheckOnce() - WARN: NULL pointer in connections vector, deleting");
                this->connections.erase(this->connections.begin() + i);
                i--;
                continue;
            }
            if(!this->connections[i]->connectivityCheck()){ // If the connectivity check fails, delete the connection
                this->servLog.add("server::connectivityCheckOnce() - INFO: Deleting connection " + std::to_string(i) + " (failed to respond to ping)");
                this->connections.erase(this->connections.begin() + i);
                if(this->selectedConnection > i){ // So we dont mess up whats currently selected in the UI
                    this->selectedConnection--;
                }
                i--;
                continue;
            }
            this->servLog.add("server::connectivityCheckOnce() - INFO: Received valid response from " + std::to_string(i));
        }
        this->connectionsMutex.unlock();
    }

    /*
        Function that the connectivity checking thread runs
        Checks if the connections are still connected by sending a ping to them
        (Hopefully) thread safe via the connectionsMutex :D
    */
    void connectivityCheckThreadFunction(){
        while(true){
            std::this_thread::sleep_for(std::chrono::seconds(connectivityCheckIntervalSeconds));
            if(!this->checkConnectivity){ // If checkConnectivity is false, dont check the connectivity of the connections
                continue;
            }
            this->servLog.add("server::connectivityCheckThreadFunction() - INFO: Checking connections via connectivityCheckOnce()");
            this->connectivityCheckOnce();
        }
    }


    /*
        Draws a menu that can be used to control a few different things
    */
    void drawMenu(float windowWidthStartPercent,
                  float windowHeightStartPercent,
                  float windowWidthEndPercent,
                  float windowHeightEndPercent,
                  ImGuiCond condition=ImGuiCond_Always){
        unsigned int menuWindowStartX = windowWidthStartPercent * ImGui::GetIO().DisplaySize.x;
        unsigned int menuWindowStartY = windowHeightStartPercent * ImGui::GetIO().DisplaySize.y;
        unsigned int menuWindowWidth = (windowWidthEndPercent * ImGui::GetIO().DisplaySize.x) - menuWindowStartX;
        unsigned int menuWindowHeight = (windowHeightEndPercent * ImGui::GetIO().DisplaySize.y) - menuWindowStartY;
        ImGui::SetNextWindowPos(ImVec2(menuWindowStartX, menuWindowStartY), condition);
        ImGui::SetNextWindowSize(ImVec2(menuWindowWidth, menuWindowHeight), condition);
        ImGui::Begin("Server Menu");

        // Log clearing buttons
        ImGui::Dummy(ImVec2(0, 1));
        if(ImGui::Button("Clear log")){
            this->servLog.clear();
        }
        ImGui::SameLine();
        if(ImGui::Button("Clear log file")){
            this->servLog.clearFile();
        }

        // Debug checkbox
        ImGui::Dummy(ImVec2(0, 1));
        ImGui::Checkbox("Extra debug windows", &this->debugEnabled);

        // Connectivity check checkbox and interval input
        ImGui::Dummy(ImVec2(0, 1));
        ImGui::Text("Connectivity check interval (s)");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth()-1.0f);
        ImGui::InputInt("##Connectivity check interval (s)", &this->connectivityCheckIntervalSeconds);
        if(this->connectivityCheckIntervalSeconds < 1){ // Will segfault if tries to check every 0 seconds
            this->connectivityCheckIntervalSeconds = 1;
        }
        ImGui::Checkbox("Enable auto connectivity check", &this->checkConnectivity);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::TextWrapped("WARN: Checking connections may freeze the interface for a bit");
        ImGui::PopStyleColor();
        // Manual connectivity check button
        if(ImGui::Button("Check connections now")){
            this->connectivityCheckOnce();
        }

        ImGui::End();
    }

    /*
        Draws connections list
    */
    void drawConnectionsList(float windowWidthStartPercent,
                             float windowHeightStartPercent,
                             float windowWidthEndPercent,
                             float windowHeightEndPercent,
                             ImGuiCond condition=ImGuiCond_Always){
        unsigned int menuWindowStartX = windowWidthStartPercent * ImGui::GetIO().DisplaySize.x;
        unsigned int menuWindowStartY = windowHeightStartPercent * ImGui::GetIO().DisplaySize.y;
        unsigned int menuWindowWidth = (windowWidthEndPercent * ImGui::GetIO().DisplaySize.x) - menuWindowStartX;
        unsigned int menuWindowHeight = (windowHeightEndPercent * ImGui::GetIO().DisplaySize.y) - menuWindowStartY;
        ImGui::SetNextWindowPos(ImVec2(menuWindowStartX, menuWindowStartY), condition);
        ImGui::SetNextWindowSize(ImVec2(menuWindowWidth, menuWindowHeight), condition);
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

    /*
        Draws debug info
    */
    void drawDebugInfo(){
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


public:

    /*
        Constructor - Sets up a socket to listen for new connections on and starts the listener thread
    */
    server(unsigned int port = 8080, unsigned int maxConnections = 32){
        this->servLog.setup("Server Log", true, "TRAServer.log");
        this->servLog.add("server::server() - INFO: Starting server");
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
        this->servLog.add("server::server() - INFO: Starting listener thread");
        std::thread listenerThread(&server::listenerThreadFunction, this);
        listenerThread.detach(); // Detaching the thread so it doesn't need to be joined

        // Start the thread which loops over every established connection (which doesnt have an active terminal) and checks if its still alive
        this->servLog.add("server::server() - INFO: Starting connectivity check thread");
        std::thread connectivityCheckThread(&server::connectivityCheckThreadFunction, this);
        connectivityCheckThread.detach();
    }

    void draw(){
        this->drawMenu(0, 0, 0.3, 0.3); // Draw the menu
        this->drawConnectionsList(0.31, 0, 1.0, 0.3); // Draw the connections list
        if(this->debugEnabled){ // If debugging is enabled, draw a window which shows a little bit of info about the server
            this->drawDebugInfo();
        }
        this->connectionsMutex.lock();
        for(int i=0; i<this->connections.size(); i++){
            if(this->connections[i]->terminalActive){
                this->connections[i]->draw(0, 0.31, 1, 0.85);
            }
        }
        this->connectionsMutex.unlock();
        this->servLog.draw(0, 0.86, 1, 1);
    }

    void update(){ // Called every frame
        this->connectionsMutex.lock(); // Lock the connections vector so no other thread can modify it while we are updating
        for(int i=0; i<this->connections.size(); i++){ // For every connection.....
            if(this->connections[i]->terminalActive && this->selectedConnection != i){ // If a connection is active, but it isnt the currently selected terminal
                this->servLog.add("server::update() - INFO: terminal " + std::to_string(i) + " closed (moved to background)");
                this->connections[i]->closeTerminal(); // Do some cleanup, because its about to be moved back to the background
            }
            this->connections[i]->terminalActive = (i==this->selectedConnection)?true:false; // Set the terminalActive variable to true if the connection is currently selected in the UI
            this->connections[i]->update(); // Update the connection
        }
        this->connectionsMutex.unlock(); // Other threads can have fun again :)
    }

};