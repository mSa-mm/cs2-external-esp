#pragma once
#include <algorithm>
#include <vector>

#include "Config.hpp"
#include "EntitySnapshot.hpp"
#include "Process.hpp"
#include "Resolve.hpp"
#include "WorldToScreen.hpp"

namespace wrench {

    // Assumed, not build-verified: generic Source-engine standing player height, in game units.
    // Adjust this first if boxes look the wrong height once you see them drawn.
    constexpr float kAssumedPlayerHeightUnits = 72.0f;

    struct ScreenEntity {
        Vec2 boxTop;      // projected head point
        Vec2 boxBottom;   // projected feet point (entity origin)
        std::int32_t health;
        std::uint8_t team;
    };

    inline SnapshotResult BuildScreenEntities(const Process& proc, const ModuleRange& client,
        const BuildConfig& cfg, const EntityHandleLayout& layout,
        int screenWidth, int screenHeight,
        std::vector<ScreenEntity>& out) {
        out.clear();

        const auto snapshot = WalkEntities(proc, client, cfg, layout);
        if (snapshot.gated) return snapshot;

        const auto viewProjAddr = ResolveRva(client, cfg.viewMatrix);
        if (!viewProjAddr) {
            SnapshotResult r; r.gated = true; r.gateReason = "ViewMatrix RVA out of module range"; return r;
        }
        const auto viewProj = proc.Read<Mat4>(*viewProjAddr);
        if (!viewProj) {
            SnapshotResult r; r.gated = true; r.gateReason = "ViewMatrix read failed (partial read)"; return r;
        }

        const auto localPawn = ResolvePointerRva(proc, client, cfg.localPlayerPawn);

        for (const auto& row : snapshot.rows) {
            if (localPawn && row.entityAddress == *localPawn) continue;

            const Vec3 feet{ row.originX, row.originY, row.originZ };
            const Vec3 head{ row.originX, row.originY, row.originZ + kAssumedPlayerHeightUnits };

            const auto feetScreen = WorldToScreen(feet, *viewProj, screenWidth, screenHeight);
            const auto headScreen = WorldToScreen(head, *viewProj, screenWidth, screenHeight);
            if (!feetScreen || !headScreen) continue;  // either point behind camera, can't box it

            // Screen-bounds cull: skip if the whole box sits outside the visible window.
            const float minX = std::min(feetScreen->x, headScreen->x);
            const float maxX = std::max(feetScreen->x, headScreen->x);
            const float minY = std::min(feetScreen->y, headScreen->y);
            const float maxY = std::max(feetScreen->y, headScreen->y);
            if (maxX < 0 || minX > screenWidth || maxY < 0 || minY > screenHeight) continue;

            out.push_back({ *headScreen, *feetScreen, row.health, row.team });
        }

        return snapshot;
    }

}  // namespace wrench