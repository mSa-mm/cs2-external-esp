#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace wrench {

    // Rva: offset from the client.dll image base. FieldOffset: offset inside a struct or class.
    enum class ValueKind { Rva, FieldOffset };

    struct ConfigValue {
        std::string_view name;
        ValueKind kind;
        std::optional<std::uint64_t> value;  // nullopt = unset, which is different from a real 0
        std::string_view source;             // build + dump file + schema entry, or "UNSET"

        bool IsSet() const noexcept { return value.has_value(); }
    };

    struct BuildConfig {
        std::string_view buildLabel = "UNSET";
        std::optional<std::uint32_t> clientTimeDateStamp;
        std::optional<std::uint32_t> clientSizeOfImage;

        ConfigValue entityList{ "EntityList",                 ValueKind::Rva,         std::nullopt, "UNSET" };
        ConfigValue gameEntitySystem{ "GameEntitySystem",           ValueKind::Rva,         std::nullopt, "UNSET" };
        ConfigValue highestEntityIndex{ "GameEntitySystem.HighestIdx", ValueKind::FieldOffset, std::nullopt, "UNSET" };
        ConfigValue localPlayerController{ "LocalPlayerController",      ValueKind::Rva,         std::nullopt, "UNSET" };
        ConfigValue localPlayerPawn{ "LocalPlayerPawn",       ValueKind::Rva,         std::nullopt, "UNSET" };
        ConfigValue viewMatrix{ "ViewMatrix",                 ValueKind::Rva,         std::nullopt, "UNSET" };
        ConfigValue controllerPawnHandle{ "Controller.PawnHandle",      ValueKind::FieldOffset, std::nullopt, "UNSET" };
        ConfigValue controllerIsLocal{ "Controller.IsLocal",         ValueKind::FieldOffset, std::nullopt, "UNSET" };
        ConfigValue entityHealth{ "Entity.Health",              ValueKind::FieldOffset, std::nullopt, "UNSET" };
        ConfigValue entityTeam{ "Entity.Team",                ValueKind::FieldOffset, std::nullopt, "UNSET" };
        ConfigValue pawnOrigin{ "Pawn.Origin",                ValueKind::FieldOffset, std::nullopt, "UNSET" };
        ConfigValue entityPtrArray{ "GameEntitySystem.PtrArray", ValueKind::FieldOffset, std::nullopt, "UNSET" };
        ConfigValue handleIndexMask{ "CHandle.IndexMask",         ValueKind::FieldOffset, std::nullopt, "UNSET" };

        std::vector<const ConfigValue*> All() const {
            return { &entityList, &gameEntitySystem, &highestEntityIndex, &localPlayerController, &localPlayerPawn,
                    &viewMatrix, &controllerPawnHandle, &controllerIsLocal,
                    &entityHealth, &entityTeam, &pawnOrigin };
        }
    };
// This is just a test
inline BuildConfig DefaultConfig_Build20261121() {
    BuildConfig c;
    c.buildLabel = "build 2026-11-21 (cs2-dumper), client.dll TimeDateStamp 0x6aff8722";
    c.clientTimeDateStamp = 0x6aff8722;
    c.clientSizeOfImage   = 0x298e000;


    c.entityList = {"EntityList", ValueKind::Rva, 0x270D5B0,
        "build 2026-11-21, offsets.hpp, client_dll::dwEntityList"};
    c.gameEntitySystem = {"GameEntitySystem", ValueKind::Rva, 0x270D5B0,
        "build 2026-11-21, offsets.hpp, client_dll::dwGameEntitySystem"};
    c.highestEntityIndex = {"GameEntitySystem.HighestIdx", ValueKind::FieldOffset, 0x2120,
        "build 2026-11-21, offsets.hpp, client_dll::dwGameEntitySystem_highestEntityIndex; live-read as 190, sane"};
    c.localPlayerController = {"LocalPlayerController", ValueKind::Rva, 0x252F300,
        "build 2026-11-21, offsets.hpp, client_dll::dwLocalPlayerController; confirmed one dereference live"};
    c.localPlayerPawn = {"LocalPlayerPawn", ValueKind::Rva, 0x2557AB8,
        "build 2026-11-21, offsets.hpp, client_dll::dwLocalPlayerPawn; confirmed one dereference live, matched pawn via handle"};

    c.controllerPawnHandle = {"Controller.PawnHandle", ValueKind::FieldOffset, 0x92C,
        "build 2026-11-21, client_dll.hpp, CCSPlayerController::m_hPlayerPawn; live-decoded index/chunk/slot on two players"};
    c.entityHealth = {"Entity.Health", ValueKind::FieldOffset, 0x34C,
        "build 2026-11-21, client_dll.hpp, C_BaseEntity::m_iHealth; live-verified 100 on two players"};
    c.entityTeam = {"Entity.Team", ValueKind::FieldOffset, 0x3E7,
        "build 2026-11-21, client_dll.hpp, C_BaseEntity::m_iTeamNum; live-verified 3 on two players"};

    c.pawnOrigin = { "Pawn.Origin", ValueKind::FieldOffset, 0x14A4,
    "build 2026-11-21, client_dll.hpp, C_BasePlayerPawn::m_vOldOrigin (unchanged from build 14199)" };
    c.viewMatrix = { "ViewMatrix", ValueKind::Rva, 0x255D3D0,
        "build 2026-11-21, offsets.hpp, client_dll::dwViewMatrix" };
    // controllerIsLocal stays UNSET for this build on purpose � LocalPlayerPawn comparison covers it.
    //

    // (not re-confirmed yet; do not reuse the 14199 values)
    return c;
}

}  // namespace wrench
