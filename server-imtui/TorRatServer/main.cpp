#include "includeCrap.hpp"
#include "server.hpp"

#define serverPort 8080

int main() {
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
    auto screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();

    // Server setup (see server.hpp)
    server serverInstance(serverPort, 16);
    serverInstance.DEBUG = false;
    
    while (true) {
        // Start the frame
        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();
        ImGui::NewFrame();

        // Do stuff!
        serverInstance.update();
        serverInstance.draw(); // Draws most of the imgui stuff (update() does some too)

        // Update and render
        ImGui::Render();
        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();
    }

    // Cleanup
    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();
    return 0;
}