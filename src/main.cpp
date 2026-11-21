#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <Windows.h>

#include "Config.hpp"
#include "Process.hpp"
#include "ScreenEntities.hpp"
#include "Overlay.hpp"
#include "Draw.hpp"

int main() {
    using namespace wrench;

    DWORD err = 0;
    auto proc = Process::Attach(L"cs2.exe", err);
    if (!proc) {
        std::wcerr << L"attach to cs2.exe failed, Win32 error " << err << L"\n";
        return 1;
    }

    auto client = proc->FindModule(L"client.dll");
    if (!client) {
        std::wcerr << L"client.dll not loaded in pid " << proc->Pid() << L" yet\n";
        return 2;
    }

    std::vector<std::byte> header(0x1000);
    if (!proc->ReadBytes(client->base, header.data(), header.size())) {
        std::wcerr << L"could not read client.dll header\n";
        return 3;
    }
    const auto pe = ParsePeHeader(header);
    if (!pe) {
        std::wcerr << L"client.dll header is not a valid 64-bit PE\n";
        return 4;
    }
    if (pe->sizeOfImage != client->size) {
        std::wcerr << L"SizeOfImage " << pe->sizeOfImage << L" does not match module size " << client->size << L"\n";
        return 5;
    }

    std::wcout << std::hex << std::showbase;
    std::wcout << L"cs2.exe pid " << std::dec << proc->Pid() << std::hex << L"\n"
        << L"client.dll base " << client->base << L" size " << client->size << L"\n"
        << L"client.dll TimeDateStamp " << pe->timeDateStamp
        << L" SizeOfImage " << pe->sizeOfImage << L"\n";

    const BuildConfig cfg = DefaultConfig_Build20261121();
    if (!cfg.clientTimeDateStamp || !cfg.clientSizeOfImage) {
        std::wcout << L"build fingerprint UNSET: pin the TimeDateStamp and SizeOfImage above in the config\n";
    }
    else if (*cfg.clientTimeDateStamp != pe->timeDateStamp || *cfg.clientSizeOfImage != pe->sizeOfImage) {
        std::wcout << L"build MISMATCH: config was made for a different client.dll, features stay gated\n";
    }
    else {
        std::wcout << L"build fingerprint MATCH: " << cfg.buildLabel.data() << L"\n";
    }

    for (const ConfigValue* v : cfg.All()) {
        std::wcout << L"  " << std::wstring(v->name.begin(), v->name.end())
            << (v->IsSet() ? L"  SET\n" : L"  UNSET (gated)\n");
    }

    Overlay overlay;
    if (!overlay.Init()) {
        std::wcerr << L"overlay window/device init failed\n";
        return 6;
    }

    const EntityHandleLayout layout{};
    std::vector<ScreenEntity> entities;

    int debugFrameCount = 0;
    overlay.Run(
        [&](int width, int height) {
            entities.clear();
            const auto snap = BuildScreenEntities(*proc, *client, cfg, layout, width, height, entities);
            DrawEntities(entities);

            if (++debugFrameCount % 180 == 0) {
                std::wcout << L"[debug] entities-seen(pre-filter)=" << std::dec << snap.debugEntitiesSeen
                    << L" players-after-health/team-filter=" << snap.rows.size()
                    << L" boxes-drawn(after screen cull)=" << entities.size() << L"\n";
            }
        },
        [&]() { return !proc->IsAlive(); }
    );

    return 0;
}