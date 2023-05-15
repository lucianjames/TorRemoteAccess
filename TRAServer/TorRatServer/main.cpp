#include <signal.h>

#include "imtui/imtui.h"
#include "imtui/imtui-impl-ncurses.h"

#include "TorPlusPlus/torplusplus.hpp"

#include "checkTor.hpp"
#include "server.hpp"

#define externPort 1337
#define internPort 52727

int main() {
    // Start TOR using torplusplus
    // Assumes tor is installed as a command
    torPlusPlus::TOR tor(9051, ".tratpptorrc");
    tor.addService("./serverService", externPort, internPort);
    tor.start();
    // tor.start() has functionality to check if tor started correctly
    // so dont need to worry about verifying it

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
    server serverInstance(internPort, 16);
    
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