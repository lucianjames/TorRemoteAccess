#pragma once

#include "imtui/imtui.h"

namespace uiHelper{

void configNextWinPosSizePercent(float windowWidthStartPercent,
                                 float windowHeightStartPercent,
                                 float windowWidthEndPercent,
                                 float windowHeightEndPercent,
                                 ImGuiCond condition=ImGuiCond_Always){
        unsigned int menuWindowStartX = windowWidthStartPercent * ImGui::GetIO().DisplaySize.x;
        unsigned int menuWindowStartY = windowHeightStartPercent * ImGui::GetIO().DisplaySize.y;
        unsigned int menuWindowWidth = (windowWidthEndPercent * ImGui::GetIO().DisplaySize.x) - menuWindowStartX;
        unsigned int menuWindowHeight = (windowHeightEndPercent * ImGui::GetIO().DisplaySize.y) - menuWindowStartY;
        ImGui::SetNextWindowPos(ImVec2(menuWindowStartX, menuWindowStartY), condition);
        ImGui::SetNextWindowSize(ImVec2(menuWindowWidth, menuWindowHeight), condition);
}

}; // namespace uiHelper