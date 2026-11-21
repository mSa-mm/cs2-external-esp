#pragma once
#include <cstdint>
#include <vector>

#include "ScreenEntities.hpp"
#include "imgui.h"

namespace wrench {

    // Team 2 = Terrorist (orange-ish), team 3 = Counter-Terrorist (blue-ish).
    // WalkEntities already filters to only team 2/3, so no default/unknown-team branch needed here.
    inline ImU32 ColorForTeam(std::uint8_t team) {
        return (team == 2) ? IM_COL32(235, 160, 50, 255)   // T
            : IM_COL32(90, 160, 235, 255);  // CT
    }

    inline void DrawEntities(const std::vector<ScreenEntity>& entities) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        for (const auto& e : entities) {
            const float boxHeight = e.boxBottom.y - e.boxTop.y;
            if (boxHeight <= 1.0f) continue;  // degenerate projection, nothing sane to draw

            const float boxWidth = boxHeight * 0.55f;  // rough box aspect, tune once visible
            const float centerX = (e.boxTop.x + e.boxBottom.x) * 0.5f;

            const ImVec2 topLeft(centerX - boxWidth * 0.5f, e.boxTop.y);
            const ImVec2 bottomRight(centerX + boxWidth * 0.5f, e.boxBottom.y);
            const ImU32 color = ColorForTeam(e.team);

            draw->AddRect(topLeft, bottomRight, color, 0.0f, 0, 1.5f);

            char label[16];
            std::snprintf(label, sizeof(label), "%d", static_cast<int>(e.health));
            const ImVec2 textSize = ImGui::CalcTextSize(label);
            const ImVec2 textPos(centerX - textSize.x * 0.5f, e.boxTop.y - textSize.y - 2.0f);
            draw->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
        }
    }

}  // namespace wrench
