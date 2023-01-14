#pragma once

#include "includeCrap.hpp"

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
        this->logMutex.lock();
        this->logMessages.push_back(m);
        if(this->writeToFile){
            std::ofstream logFile(this->logFile, std::ios::app);
            logFile << m << std::endl;
            logFile.close();
        }
        this->logMutex.unlock();
    }

    void draw(){
        // Set up the window
        ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x/2)-(windowWidth/2), (ImGui::GetIO().DisplaySize.y/2)-(windowHeight/2)), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Once);
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

};