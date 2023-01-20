#include <signal.h>

#include "imtui/imtui.h"
#include "imtui/imtui-impl-ncurses.h"

#include "checkTor.hpp"
#include "server.hpp"

#define serverPort 52727

int main() {
    if(!torRunning()){
        printf("Tor is not running! Please start tor before running this program.\n(Failed to connect to 127.0.0.1:9050)\n");
        return 1;
    }

    // Make all threads ignore SIGPIPE
    // This is so that if a connection is closed, the program doesn't crash
    signal(SIGPIPE, SIG_IGN);
    sigset_t sigpipeMask;
    sigemptyset(&sigpipeMask);
    sigaddset(&sigpipeMask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &sigpipeMask, NULL);
    
    // ImTui setup:
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImTui::TScreen* screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();

    // Server setup (see server.hpp)
    server serverInstance(serverPort, 16);
    
    while(!ImGui::IsKeyPressed(27)){ // Exit cleanly if esc pressed
        // Start the frame
        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();
        ImGui::NewFrame();

        serverInstance.update(); // Updates connections
        serverInstance.draw(); // Draws pretty windows

        // Render the frame to the terminal
        ImGui::Render();
        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();
    }

    // Cleanup
    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();
    return 0;
}