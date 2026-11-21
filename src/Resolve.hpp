#pragma once
#include <cstdint>
#include <limits>
#include <optional>

#include "Config.hpp"
#include "Process.hpp"

namespace wrench {

    // Runtime address of an Rva entry. nullopt if unset, wrong kind, or outside the module.
    inline std::optional<std::uintptr_t> ResolveRva(const ModuleRange& module, const ConfigValue& v) noexcept {
        if (v.kind != ValueKind::Rva || !v.value || *v.value >= module.size) return std::nullopt;
        return module.base + static_cast<std::uintptr_t>(*v.value);
    }

    // Address of a field inside an object. nullopt if unset, wrong kind, null object, or overflow.
    inline std::optional<std::uintptr_t> FieldAddress(std::uintptr_t objectBase, const ConfigValue& v) noexcept {
        if (v.kind != ValueKind::FieldOffset || !v.value || objectBase == 0) return std::nullopt;
        if (*v.value > std::numeric_limits<std::uintptr_t>::max() - objectBase) return std::nullopt;
        return objectBase + static_cast<std::uintptr_t>(*v.value);
    }

    // For "dw" values that are themselves a pointer-to-object, not the object. Reads the pointer
    // at the RVA, then validates the result isn't null before handing it back. Does not check
    // the result against any module range, since heap objects live outside client.dll.
    inline std::optional<std::uintptr_t> ResolvePointerRva(const Process& proc, const ModuleRange& module,
        const ConfigValue& v) noexcept {
        const auto slot = ResolveRva(module, v);
        if (!slot) return std::nullopt;
        const auto target = proc.Read<std::uint64_t>(*slot);
        if (!target || *target == 0) return std::nullopt;
        return static_cast<std::uintptr_t>(*target);
    }

}  // namespace wrench