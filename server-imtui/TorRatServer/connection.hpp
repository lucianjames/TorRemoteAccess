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
    bool terminalActive = false;
    std::vector<std::string> plainTextMessageHistory;
    int selectedMessage = 0;
    char inputBuffer[1024] = {0};
    std::string msgToSend = "";

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

    void drawTerminalWindow(){
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(35, 10), ImGuiCond_Once);
        ImGui::Begin(("Socket " + std::to_string(this->sockFd) + " terminal").c_str());
        ImGui::BeginChild("Scrolling", ImVec2(0, -3), false);
        for(auto m : this->plainTextMessageHistory){
            ImGui::TextWrapped("%s", m.c_str());
        }
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()){
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::Separator();
        if(ImGui::InputText("Input", this->inputBuffer, 1024, ImGuiInputTextFlags_EnterReturnsTrue)){
            this->msgToSend = std::string(this->inputBuffer);
        }
        ImGui::End();
    }

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

    void update(){
        if(this->terminalActive){
            this->drawTerminalWindow();
        }
        if(this->msgToSend != ""){
            this->sendMsgPlaintext(this->msgToSend);
        }
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