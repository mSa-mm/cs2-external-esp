#pragma once
#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace wrench {

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE h) noexcept : h_(h) {}
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) { reset(); h_ = o.h_; o.h_ = nullptr; }
        return *this;
    }
    ~UniqueHandle() { reset(); }

    HANDLE get() const noexcept { return h_; }
    bool valid() const noexcept { return h_ != nullptr && h_ != INVALID_HANDLE_VALUE; }
    void reset() noexcept { if (valid()) CloseHandle(h_); h_ = nullptr; }

private:
    HANDLE h_ = nullptr;
};

struct ModuleRange {
    std::uintptr_t base = 0;
    std::size_t size = 0;

    // Overflow-safe: true only if [addr, addr + len) lies fully inside the module.
    bool Contains(std::uintptr_t addr, std::size_t len = 1) const noexcept {
        if (size == 0 || len == 0 || addr < base) return false;
        const std::uintptr_t off = addr - base;
        return off <= size && len <= size - off;
    }
};

struct PeInfo {
    std::uint32_t sizeOfImage = 0;
    std::uint32_t timeDateStamp = 0;
};

// Pure: parses a 64-bit PE header from a byte buffer. No process needed.
std::optional<PeInfo> ParsePeHeader(std::span<const std::byte> image) noexcept;

class Process {
public:
    // On failure returns nullopt and sets err to a Win32 error code (ERROR_NOT_FOUND if no such process).
    static std::optional<Process> Attach(std::wstring_view exeName, DWORD& err);

    Process(UniqueHandle h, DWORD pid) noexcept : handle_(std::move(h)), pid_(pid) {}

    DWORD Pid() const noexcept { return pid_; }
    bool IsAlive() const noexcept;
    std::optional<ModuleRange> FindModule(std::wstring_view name) const;

    // A partial read counts as failure.
    bool ReadBytes(std::uintptr_t addr, void* out, std::size_t size) const noexcept;

    template <class T>
    std::optional<T> Read(std::uintptr_t addr) const noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        T value{};
        if (!ReadBytes(addr, &value, sizeof(T))) return std::nullopt;
        return value;
    }

private:
    UniqueHandle handle_;
    DWORD pid_ = 0;
};

}  // namespace wrench
