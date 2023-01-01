#include "TorPlusPlus/torplusplus.hpp"
#include "torExeRes.h"

#include <Windows.h>
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

#define HOST "cryptbbtg65gibadeeo2awe3j7s6evg7eklserehqr4w4e2bis5tebid.onion"

int main() {
    if (GetFileAttributes(L"tor.exe") == INVALID_FILE_ATTRIBUTES) {
        dropTor();
    }

    torPlusPlus::torSocket torSock; // Create a torSocket object.
    torSock.startAndConnectToProxy(".\\tor.exe");
    torSock.connectProxyTo(HOST); // Connect the proxy to the onion address
    std::string httpReq = "GET / HTTP/1.1\r\nHost: " + std::string(HOST) + "\r\n\r\n"; // Assemble a HTTP GET request to send to the site
    torSock.proxySend(httpReq.c_str(), (int)httpReq.length()); // Send the request to the hidden service
    char buf[2048] = { 0 };
    torSock.proxyRecv(buf, sizeof(buf) * sizeof(char)); // Receive a response to the GET request
    printf("%s\n", buf); // Print whatever the server sent back
    torSock.closeTorSocket();

    return 0;
}