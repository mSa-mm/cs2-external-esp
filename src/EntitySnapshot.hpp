#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "Config.hpp"
#include "Process.hpp"
#include "Resolve.hpp"

namespace wrench {

    // Derived live from a matched PawnHandle/slot pairing, build 2026-11-21 dump, not assumed.
    struct EntityHandleLayout {
        std::uint32_t indexMask = 0x7FFF;
        std::uint32_t slotBits = 9;     // slot = index & ((1 << slotBits) - 1)
        std::uint32_t slotMask = 0x1FF;
    };

    struct EntitySnapshotRow {
        std::uintptr_t entityAddress = 0;
        std::int32_t health = 0;
        std::uint8_t team = 0;
        float originX = 0, originY = 0, originZ = 0;
    };

    struct SnapshotResult {
        std::vector<EntitySnapshotRow> rows;
        bool gated = false;
        const char* gateReason = "";
        int debugEntitiesSeen = 0;   // non-null entity pointers found, before health/team filtering
        int debugHealthPassed = 0;   // health in 1-100, regardless of team
        int debugTeamPassed = 0;     // team is 2 or 3, regardless of health
    };

    constexpr std::uintptr_t kChunkArrayOffset = 0x10;   // GameEntitySystem + this = first chunk pointer
    constexpr std::uintptr_t kChunkEntryStride = 0x70;   // bytes between entries inside one chunk
    constexpr std::uintptr_t kEntryEntityPtrOffset = 0x00;
    constexpr std::uintptr_t kEntryHandleOffset = 0x10;

    inline SnapshotResult WalkEntities(const Process& proc, const ModuleRange& client,
        const BuildConfig& cfg, const EntityHandleLayout& layout) {
        SnapshotResult result;

        if (!cfg.entityHealth.IsSet() || !cfg.entityTeam.IsSet()) {
            result.gated = true;
            result.gateReason = "Entity.Health / Entity.Team UNSET for this build: paste this build's C_BaseEntity block";
            return result;
        }

        const auto systemAddr = ResolvePointerRva(proc, client, cfg.gameEntitySystem);
        if (!systemAddr) { result.gated = true; result.gateReason = "GameEntitySystem pointer null or unreadable"; return result; }

        const auto highestIdxAddr = FieldAddress(*systemAddr, cfg.highestEntityIndex);
        if (!highestIdxAddr) { result.gated = true; result.gateReason = "HighestIdx field address invalid"; return result; }

        const auto highestIdx = proc.Read<std::int32_t>(*highestIdxAddr);
        if (!highestIdx || *highestIdx < 0 || *highestIdx > 4096) {
            result.gated = true; result.gateReason = "HighestIdx read failed or implausible"; return result;
        }

        std::int32_t lastChunk = -1;
        std::optional<std::uintptr_t> chunkPtr;

        for (std::int32_t index = 0; index <= *highestIdx; ++index) {
            const std::int32_t chunk = index >> layout.slotBits;
            const std::int32_t slot = index & static_cast<std::int32_t>(layout.slotMask);

            if (chunk != lastChunk) {
                lastChunk = chunk;
                const auto chunkSlotAddr = *systemAddr + kChunkArrayOffset + static_cast<std::uintptr_t>(chunk) * 8;
                const auto raw = proc.Read<std::uint64_t>(chunkSlotAddr);
                chunkPtr = (raw && *raw != 0) ? std::optional<std::uintptr_t>(static_cast<std::uintptr_t>(*raw)) : std::nullopt;
            }
            if (!chunkPtr) continue;

            const auto entryAddr = *chunkPtr + static_cast<std::uintptr_t>(slot) * kChunkEntryStride;
            const auto entityPtr = proc.Read<std::uint64_t>(entryAddr + kEntryEntityPtrOffset);
            if (!entityPtr || *entityPtr == 0) continue;

            const auto entityAddr = static_cast<std::uintptr_t>(*entityPtr);
            ++result.debugEntitiesSeen;   // counts every non-null entity, player or not, pre-filter

            const auto healthAddr = FieldAddress(entityAddr, cfg.entityHealth);
            const auto teamAddr = FieldAddress(entityAddr, cfg.entityTeam);
            const auto originAddr = FieldAddress(entityAddr, cfg.pawnOrigin);
            if (!healthAddr || !teamAddr || !originAddr) continue;

            const auto health = proc.Read<std::int32_t>(*healthAddr);
            const auto team = proc.Read<std::uint8_t>(*teamAddr);
            const auto ox = proc.Read<float>(*originAddr);
            const auto oy = proc.Read<float>(*originAddr + 4);
            const auto oz = proc.Read<float>(*originAddr + 8);
            if (!health || !team || !ox || !oy || !oz) continue;
            if (!std::isfinite(*ox) || !std::isfinite(*oy) || !std::isfinite(*oz)) continue;

            if (!std::isfinite(*ox) || !std::isfinite(*oy) || !std::isfinite(*oz)) continue;

            // Plausibility filter: this walk hits every entity, not just player pawns, and
            // Entity.Health/Entity.Team/Pawn.Origin only mean something on an actual pawn.
            // A real CS2 player is health 1-100 and team 2 (T) or 3 (CT); anything else here
            // is a weapon, grenade, trigger volume, or similar non-pawn entity, not a player.
            if (*health < 1 || *health > 100) continue;
            if (*team != 2 && *team != 3) continue;

            EntitySnapshotRow row;
            row.entityAddress = entityAddr;
            row.health = *health; row.team = *team;
            row.originX = *ox; row.originY = *oy; row.originZ = *oz;

            result.rows.push_back(row);
        }

        return result;
    }

}  // namespace wrench