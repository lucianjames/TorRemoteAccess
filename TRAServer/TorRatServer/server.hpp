#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>

#include "connection.hpp"
#include "logWindow.hpp"
#include "uiHelper.hpp"

#define perror_exit(msg) perror(msg); exit(EXIT_FAILURE);

/*
    The server class manages new connections and checking the status of existing connections
*/
class server{
private:
    std::vector<std::unique_ptr<connection>> connections;
    std::mutex connectionsMutex; // To ensure connections vect isnt modified by two threads at once

    int serverListenerFd; // Socket file descriptor for the server to listen for new connections on
    struct sockaddr_in listenerAddress; // Used to configure the listening socket
    int listenerOpt = 1; // Used for setsockopt
    unsigned int maxConnections; // Maximum number of connections to the server

    bool fixedLayout = true;
    bool checkConnectivity = false; // If true, the server will check the connectivity of all connections every connectivityCheckInterval seconds
    int connectivityCheckInterval = 10; // Interval between connectivity checks
    int selectedConnection = 0; // Index of the selected connection in the connections vector
    logWindow servLog; // Log window for the server



    /*
        Function that the listener thread runs
        Accepts new connections if this->connections.size < this->maxConnections
        Runs the initialConnection() function on new connections before adding them to this->connections,
        which verifies that the connection is valid.
        This function will run forever (while(true))
    */
    void listenerThreadFunction(){
        while(true){
            if(this->connections.size() < this->maxConnections){ // Only accept a new connection if the maximum connection count hasnt been reached
                // Wait for a new connection:
                this->servLog.add("server::listenerThreadFunction() - INFO: Waiting for new connection");
                int newConnectionFd; // The file descriptor for the new connection, will be set by accept() then used to instantiate a new connection object
                struct sockaddr_in newAddress; // Will also be set by accept(), but isnt actually used.
                int newAddrlen = sizeof(newAddress);
                if((newConnectionFd = accept(this->serverListenerFd, (struct sockaddr *)&newAddress, (socklen_t*)&newAddrlen))<0){
                    perror_exit("server::listenerThreadFunction() - ERR: accept() failed");
                }
                this->servLog.add("server::listenerThreadFunction() - INFO: New connection accepted");

                // Create a unique_ptr to a new connection object for the new connection
                // A unique_ptr will automatically free when no longer used, very cool.
                std::unique_ptr<connection> newConnection = std::make_unique<connection>(newConnectionFd, &this->servLog);

                // Verify the connection using the intialConnection() function (part of the connection class)
                this->servLog.add("server::listenerThreadFunction() - INFO: Verifying connection");
                if(newConnection->intialConnection()){ // initialConnection returns true if all the right data is received from the client
                    this->servLog.add("server::listenerThreadFunction() - INFO: Connection verified - adding to the list of connections");
                    this->connectionsMutex.lock(); // Make sure this thread is the only one using the connections vector
                    this->connections.push_back(std::move(newConnection)); // Move ownership of the unique_ptr<connection> to the connections vector
                    this->connectionsMutex.unlock();
                }else{
                    this->servLog.add("server::listenerThreadFunction() - WARN: Connection failed verification, discarding");
                }
            }else{
                // Sleep for a bit to reduce CPU usage.
                // Not sleeping would result in the if statement being called a lot of times really fast
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
            }
        }
    }

    /*
        Used to check connectivity just once, does not ignore active terminals
    */
    void connectivityCheckOnce(){
        this->servLog.add("server::connectivityCheckOnce() - INFO: Checking connections...");
        this->connectionsMutex.lock();
        for(int i=this->connections.size()-1; i>=0; i--){ // Iterating backwards so erasing items doesnt require decrementing the index
            this->servLog.add("server::connectivityCheckOnce() - INFO: Checking connection " + std::to_string(i));
            if(this->connections[i]->connectivityCheck()){ // connectivityCheck returns true if the client is still connected and a valid response was received
                this->servLog.add("server::connectivityCheckOnce() - INFO: Received valid response from " + std::to_string(i));
            }else{ // If conn check fails, remove the connection from the vector
                this->servLog.add("server::connectivityCheckOnce() - INFO: Deleting connection " + std::to_string(i) + " (failed to respond to ping)");
                this->connections.erase(this->connections.begin() + i);
                if(this->selectedConnection > i){ // Prevents UI selection from messing up
                    this->selectedConnection--;
                }
            }
        }
        this->connectionsMutex.unlock();
    }

    /*
        Function that the connectivity checking thread runs
        Checks if the connections are still connected by sending a ping to them
        This function runs forever (while(true))
    */
    void connectivityCheckThreadFunction(){
        while(true){
            std::this_thread::sleep_for(std::chrono::seconds(connectivityCheckInterval));
            if(this->checkConnectivity){
                this->servLog.add("server::connectivityCheckThreadFunction() - INFO: Checking connections via connectivityCheckOnce()");
                this->connectivityCheckOnce();
            }
        }
    }

    /*
        Draws a menu that can be used to control a few different things
    */
    void drawMenu(float wStartXNorm,
                  float wStartYNorm,
                  float wEndXNorm,
                  float wEndYNorm,
                  ImGuiCond wCondition=ImGuiCond_Always){
        uiHelper::setNextWindowSizeNormalised(wStartXNorm,
                                              wStartYNorm,
                                              wEndXNorm,
                                              wEndYNorm,
                                              wCondition);
        ImGui::Begin("Server Menu");

        // Log clearing buttons:
        ImGui::Dummy(ImVec2(0, 1));
        if(ImGui::Button("Clear log")){
            this->servLog.clear();
        }
        ImGui::SameLine();
        if(ImGui::Button("Clear log file")){
            this->servLog.clearFile();
        }

        // Fixed layout checkbox:
        ImGui::Dummy(ImVec2(0, 1));
        ImGui::Checkbox("Fixed layout", &this->fixedLayout);

        // Connectivity check checkbox and interval input:
        ImGui::Dummy(ImVec2(0, 1));
        ImGui::Checkbox("Auto conn check", &this->checkConnectivity);
        if(this->checkConnectivity){
            ImGui::Text("Conn check interval (s)");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth()-1.0f);
            ImGui::InputInt("##Connectivity check interval (s)", &this->connectivityCheckInterval);
            if(this->connectivityCheckInterval < 1){ // Will segfault if tries to check every 0 seconds
                this->connectivityCheckInterval = 1;
            }
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvailWidth()-1.0f);
        ImGui::TextWrapped("WARN: Connections check may freeze the interface for a second or two");
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();

        // Manual connectivity check button:
        if(ImGui::Button("Check connections now")){
            this->connectivityCheckOnce();
        }

        ImGui::End();
    }

    /*
        Draws connections list
    */
    void drawConnectionsList(float wStartXNorm,
                             float wStartYNorm,
                             float wEndXNorm,
                             float wEndYNorm,
                             ImGuiCond wCondition=ImGuiCond_Always){
        uiHelper::setNextWindowSizeNormalised(wStartXNorm,
                                              wStartYNorm,
                                              wEndXNorm,
                                              wEndYNorm,
                                              wCondition);
        ImGui::Begin("Server Connections");

        // Have to make a vector of strings to pass to the ListBox function
        // Because it requires a pointer to the stuff to write onto the screen >:(
        std::vector<std::string> connInfoStrings; // Creating this every single frame is not very efficient, but its not really a problem
        if(connInfoStrings.size() == 0){
            connInfoStrings.push_back("No connections"); // This prevents the listbox from looking weird when there are no connections
        }else{
            this->connectionsMutex.lock(); // Make sure the connections arent being modified while the connInfoStrings are being assembled
            for(auto &c : this->connections){
                connInfoStrings.push_back(c->username + "@" + c->hostname + " (" + c->publicIp + ") on socket " + std::to_string(c->sockFd));
            }
            this->connectionsMutex.unlock();
        }

        // Create a listbox with the strings that were just created:
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        ImGui::ListBox("##Connections", 
                       &this->selectedConnection, 
                       [](void* data, int idx, const char** out_text){
                            std::vector<std::string>* infoStringsPtr = (std::vector<std::string>*)data; // Cast the void* to an std::vector<std::string>*
                            *out_text = infoStringsPtr->at(idx).c_str(); // Get a pointer to the string at the current index (idx)
                            return true;
                       },
                       (void*)&connInfoStrings, // This is the void* data variable of the lambda function
                       connInfoStrings.size(),
                       connInfoStrings.size()
        );

        ImGui::End();
    }
    
    /*
        Displays some info about commands and stuff
    */
    void drawHelp(float wStartXNorm,
                  float wStartYNorm,
                  float wEndXNorm,
                  float wEndYNorm,
                  ImGuiCond wCondition=ImGuiCond_Always){
        uiHelper::setNextWindowSizeNormalised(wStartXNorm,
                                              wStartYNorm,
                                              wEndXNorm,
                                              wEndYNorm,
                                              wCondition);
        ImGui::Begin("Info");
        ImGui::TextWrapped("All commands that would be available on a typical windows terminal are available, but there are some extra commands:");
        ImGui::TextWrapped("grab <path> - Grabs a file from the client and saves it to the server at the current directory");
        ImGui::TextWrapped("upload <path> - Uploads a file from the server to the client at the current directory of the client program");
        ImGui::TextWrapped("clear - Clears the terminal window");
        ImGui::Dummy(ImVec2(0, 1));
        ImGui::TextWrapped("You will also find that a few commands also have linux-like aliases (ls -> dir, rm -> del, etc.)");
        ImGui::Dummy(ImVec2(0, 1));
        ImGui::TextWrapped("Press the escape key to cleanly exit the program");
        ImGui::End();
    }



public:

    /*
        Constructor - Sets up a socket to listen for new connections on and starts the listener thread
    */
    server(unsigned int port = 8080, unsigned int maxConnections = 32){
        this->maxConnections = maxConnections;
        this->servLog.setup("Server Log", true, "TRAServer.log");
        this->servLog.add("server::server() - INFO: Starting server");

        // Creating the socket file descriptor:
        if((this->serverListenerFd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
            perror_exit("server::server() - ERR: socket() failed");
        }

        // Set up the socket options:
        if(setsockopt(this->serverListenerFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &this->listenerOpt, sizeof(this->listenerOpt))){
            perror_exit("server::server() - ERR: setsockopt() failed");
        }
        this->listenerAddress.sin_family = AF_INET;
        this->listenerAddress.sin_addr.s_addr = INADDR_ANY;
        this->listenerAddress.sin_port = htons(port);

        // Bind the socket to the address we just set up:
        if(bind(this->serverListenerFd, (struct sockaddr *)&this->listenerAddress, sizeof(this->listenerAddress))<0){
            perror_exit("server::server() - ERR: bind() failed");
        }

        // Make the socket listen for connections:
        if(listen(this->serverListenerFd, 3) < 0){ // 3 is the maximum number of pending connections, any more and the client will get a "Connection refused" error
            perror_exit("server::server() - ERR: listen() failed");
        }

        // Start up the thread that listens for and sets up connections:
        this->servLog.add("server::server() - INFO: Starting listener thread");
        std::thread listenerThread(&server::listenerThreadFunction, this);
        listenerThread.detach(); // Detaching the thread so it doesn't need to be joined

        // Start the thread which loops over every established connection (which doesnt have an active terminal) and checks if its still alive:
        this->servLog.add("server::server() - INFO: Starting connectivity check thread");
        std::thread connectivityCheckThread(&server::connectivityCheckThreadFunction, this);
        connectivityCheckThread.detach();
    }

    /*
        Draws all the windows!
        The funny numbers determine where the windows go, in terms of normalised screen coords
        (0, 0)      (0, 1)

        (1, 0)      (1, 1)
    */
    void draw(){
        this->drawMenu(0, 0, 0.3, 0.3, (this->fixedLayout)?ImGuiCond_Always:ImGuiCond_Once);
        this->drawConnectionsList(0.31, 0, 1.0, 0.3, (this->fixedLayout)?ImGuiCond_Always:ImGuiCond_Once);
        this->connectionsMutex.lock();
        for(int i=0; i<this->connections.size(); i++){ // Draws every active terminal, but its usually just one.
            if(this->connections[i]->terminalActive){
                this->connections[i]->draw(0, 0.31, 1, 0.85, (this->fixedLayout)?ImGuiCond_Always:ImGuiCond_Once);
            }
        }
        this->connectionsMutex.unlock();
        this->servLog.draw(0, 0.86, 0.75, 1, (this->fixedLayout)?ImGuiCond_Always:ImGuiCond_Once);
        this->drawHelp(0.76, 0.86, 1, 1, (this->fixedLayout)?ImGuiCond_Always:ImGuiCond_Once);
    }

    /*
        Updates all the connections
    */
    void update(){ // Called every frame
        this->connectionsMutex.lock(); // Lock the connections vector so no other thread can modify it while we are updating
        for(auto& c : this->connections){
            bool cIsSelected = this->connections[this->selectedConnection] == c;
            if(c->terminalActive && !cIsSelected){ // If a connection is active, but it isnt the currently selected terminal
                this->servLog.add("server::update() - INFO: terminal closed (moved to background)");
                c->closeTerminal(); // Do some cleanup, because its about to be moved back to the background
            }
            c->terminalActive = cIsSelected; // Make terminal active if selected
            c->update(); // Update the connection
        }
        this->connectionsMutex.unlock(); // Other threads can have fun again :)
    }

};