#pragma once

#include <ctime>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <thread>

#include "imtui/imtui.h"

#include "uiHelper.hpp"


class logWindow{
private:
    std::vector<std::string> logMessages;
    unsigned int windowWidth;
    unsigned int windowHeight;
    std::string title;
    bool writeToFile = false;
    std::string logFile;

    std::string getTime(){
        std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string tStr = std::ctime(&t);
        return tStr.substr(0, tStr.length()-1); // Remove newline
    }

public:
    std::mutex logMutex; // This is used to lock the logMessages vector when it is being modified
    // This is because multiple threads will be trying to add to the log at the same time

    void setup(std::string title="Log",
               bool writeToFile=false,
               std::string logFile="log.txt",
               unsigned int windowWidth=80, 
               unsigned int windowHeight=10){
        this->title = title;
        this->writeToFile = writeToFile;
        this->logFile = logFile;
        this->windowWidth = windowWidth;
        this->windowHeight = windowHeight;
        if(this->writeToFile){
            std::ofstream logFile(this->logFile, std::ios::app);
            logFile << "\n\n\n\n===== New Log start (" << getTime() << ") =====" << std::endl;
        }
    }

    void add(std::string m){
        std::string this_id = std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id())); // wacky copilot code
        this->logMutex.lock();
        this->logMessages.push_back(m);
        // Add more extra info to the message that gets put on disk:
        m = "[THREAD ID: " + this_id + "] (" + getTime() + ") " + m;
        if(this->writeToFile){
            std::ofstream logFile(this->logFile, std::ios::app);
            logFile << m << std::endl;
            logFile.close();
        }
        this->logMutex.unlock();
    }

    void draw(float wWidthStartPercent,
              float wHeightStartPercent,
              float wWidthEndPercent,
              float wHeightEndPercent,
              ImGuiCond condition=ImGuiCond_Always){
        uiHelper::configNextWinPosSizePercent(wWidthStartPercent,
                                              wHeightStartPercent,
                                              wWidthEndPercent,
                                              wHeightEndPercent,
                                              condition);
        ImGui::Begin(title.c_str());

        // Draw the scrolling text box, adding each item from this->logMessages:
        ImGui::BeginChild("Scrolling", ImVec2(0, -2), false);
        for(int i=0; i<logMessages.size(); i++){
            ImGui::TextWrapped("%d: %s", i, logMessages[i].c_str());
        }
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()){ // If the window is scrolled to the bottom, scroll down automatically when new text is added
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void clear(){
        this->logMutex.lock();
        this->logMessages.clear();
        this->logMutex.unlock();
    }

    void clearFile(){
        std::ofstream logFile(this->logFile, std::ios::trunc);
        logFile << "===== Log cleared (" << getTime() << ") =====" << std::endl;
        logFile.close();
    }

};