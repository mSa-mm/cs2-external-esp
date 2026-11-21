#include "Process.hpp"

#include <TlHelp32.h>

#include <cstring>
#include <cwchar>
#include <string>

namespace wrench {

namespace {
bool EqualsNoCase(std::wstring_view a, const wchar_t* b) {
    return _wcsicmp(std::wstring(a).c_str(), b) == 0;
}
}  // namespace

std::optional<PeInfo> ParsePeHeader(std::span<const std::byte> image) noexcept {
    if (image.size() < sizeof(IMAGE_DOS_HEADER)) return std::nullopt;
    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, image.data(), sizeof dos);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) return std::nullopt;

    const auto ntOff = static_cast<std::size_t>(dos.e_lfanew);
    if (ntOff > image.size() || image.size() - ntOff < sizeof(IMAGE_NT_HEADERS64)) return std::nullopt;

    IMAGE_NT_HEADERS64 nt{};
    std::memcpy(&nt, image.data() + ntOff, sizeof nt);
    if (nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return std::nullopt;

    return PeInfo{nt.OptionalHeader.SizeOfImage, nt.FileHeader.TimeDateStamp};
}

std::optional<Process> Process::Attach(std::wstring_view exeName, DWORD& err) {
    err = ERROR_NOT_FOUND;
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap.valid()) { err = GetLastError(); return std::nullopt; }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof pe;
    DWORD pid = 0;
    for (BOOL ok = Process32FirstW(snap.get(), &pe); ok; ok = Process32NextW(snap.get(), &pe)) {
        if (EqualsNoCase(exeName, pe.szExeFile)) { pid = pe.th32ProcessID; break; }
    }
    if (pid == 0) return std::nullopt;

    UniqueHandle h(OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid));
    if (!h.valid()) { err = GetLastError(); return std::nullopt; }

    err = ERROR_SUCCESS;
    return Process(std::move(h), pid);
}

bool Process::IsAlive() const noexcept {
    return handle_.valid() && WaitForSingleObject(handle_.get(), 0) == WAIT_TIMEOUT;
}

std::optional<ModuleRange> Process::FindModule(std::wstring_view name) const {
    UniqueHandle snap;
    for (int attempt = 0; attempt < 5; ++attempt) {  // ERROR_BAD_LENGTH happens while modules load
        snap = UniqueHandle(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_));
        if (snap.valid()) break;
        if (GetLastError() != ERROR_BAD_LENGTH) return std::nullopt;
    }
    if (!snap.valid()) return std::nullopt;

    MODULEENTRY32W me{};
    me.dwSize = sizeof me;
    for (BOOL ok = Module32FirstW(snap.get(), &me); ok; ok = Module32NextW(snap.get(), &me)) {
        if (EqualsNoCase(name, me.szModule)) {
            return ModuleRange{reinterpret_cast<std::uintptr_t>(me.modBaseAddr), me.modBaseSize};
        }
    }
    return std::nullopt;
}

bool Process::ReadBytes(std::uintptr_t addr, void* out, std::size_t size) const noexcept {
    if (!handle_.valid() || out == nullptr || size == 0) return false;
    SIZE_T got = 0;
    const BOOL ok = ReadProcessMemory(handle_.get(), reinterpret_cast<LPCVOID>(addr), out, size, &got);
    return ok && got == size;
}

}  // namespace wrench
