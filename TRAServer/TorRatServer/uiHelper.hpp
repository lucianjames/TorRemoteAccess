#pragma once

#include "imtui/imtui.h"

namespace uiHelper{

void configNextWinPosSizePercent(float windowWidthStartPercent,
                                 float windowHeightStartPercent,
                                 float windowWidthEndPercent,
                                 float windowHeightEndPercent,
                                 ImGuiCond condition=ImGuiCond_Always){
        int menuWindowStartX = windowWidthStartPercent * ImGui::GetIO().DisplaySize.x;
        int menuWindowStartY = windowHeightStartPercent * ImGui::GetIO().DisplaySize.y;
        int menuWindowWidth = (windowWidthEndPercent * ImGui::GetIO().DisplaySize.x) - menuWindowStartX;
        int menuWindowHeight = (windowHeightEndPercent * ImGui::GetIO().DisplaySize.y) - menuWindowStartY;
        ImGui::SetNextWindowPos(ImVec2(menuWindowStartX, menuWindowStartY), condition);
        ImGui::SetNextWindowSize(ImVec2(menuWindowWidth, menuWindowHeight), condition);
}

}; // namespace uiHelper