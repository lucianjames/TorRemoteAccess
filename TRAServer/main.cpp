#define GLFW_INCLUDE_NONE

#include <signal.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <thread>

#include "checkTor.hpp"
#include "server.hpp"


#define serverPort 52727



void message_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                        GLsizei length, GLchar const *message,
                        void const *user_param){
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION){
    return;
    }

    auto const src_str = [source](){
    switch(source){
        case GL_DEBUG_SOURCE_API:
            return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            return "WINDOW SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            return "SHADER COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            return "THIRD PARTY";
        case GL_DEBUG_SOURCE_APPLICATION:
            return "APPLICATION";
        case GL_DEBUG_SOURCE_OTHER:
            return "OTHER";
        default:
            return "UNKNOWN SOURCE";
    }
    }();

    auto const type_str = [type](){
    switch(type){
        case GL_DEBUG_TYPE_ERROR:
            return "ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            return "DEPRECATED_BEHAVIOR";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            return "UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY:
            return "PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE:
            return "PERFORMANCE";
        case GL_DEBUG_TYPE_MARKER:
            return "MARKER";
        case GL_DEBUG_TYPE_OTHER:
            return "OTHER";
        default:
            return "UNKNOWN TYPE";
    }
    }();

    auto const severity_str = [severity](){
    switch(severity){
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return "NOTIFICATION";
        case GL_DEBUG_SEVERITY_LOW:
            return "LOW";
        case GL_DEBUG_SEVERITY_MEDIUM:
            return "MEDIUM";
        case GL_DEBUG_SEVERITY_HIGH:
            return "HIGH";
        default:
            return "UNKNOWN SEVERITY";
    }
    }();
    std::cout << src_str << ", " << type_str << ", " << severity_str << ", " << id << ": " << message << '\n';
}

int main(int argc, char *argv[]){
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

    // Server setup (see server.hpp)
    server serverInstance(serverPort, 16);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    auto window = glfwCreateWindow(800, 600, "Example", nullptr, nullptr);
    if(!window){
        throw std::runtime_error("Error creating glfw window");
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if(!gladLoaderLoadGL()){
        throw std::runtime_error("Error initializing glad");
    }

    glDebugMessageCallback(message_callback, nullptr);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450 core");
    ImGui::StyleColorsClassic();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
            glfwSetWindowShouldClose(window, true);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        serverInstance.update(); // Updates connections
        serverInstance.draw(); // Draws pretty windows

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glClear(GL_COLOR_BUFFER_BIT);
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Not sure why this is here but someone clearly added it for a reason so ill keep it lol
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
