#include "Overlay.hpp"

#include <d3d11.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace wrench {

    namespace {
        Overlay* g_instance = nullptr;  // WndProc is a static callback, needs a way back to the instance
    }

    LRESULT CALLBACK Overlay::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;

        switch (msg) {
        case WM_SIZE:
            if (g_instance && wparam != SIZE_MINIMIZED) {
                g_instance->pendingResize_ = true;
                g_instance->pendingWidth_ = LOWORD(lparam);
                g_instance->pendingHeight_ = HIWORD(lparam);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    Overlay::~Overlay() {
        if (hwnd_) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            CleanupDeviceAndSwapChain();
            DestroyWindow(hwnd_);
        }
        g_instance = nullptr;
    }

    bool Overlay::Init() {
        g_instance = this;

        width_ = GetSystemMetrics(SM_CXSCREEN);
        height_ = GetSystemMetrics(SM_CYSCREEN);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Overlay::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"Cs2EspOverlayWnd";
        RegisterClassExW(&wc);

        // WS_EX_LAYERED + WS_EX_TRANSPARENT: click-through. WS_EX_TOPMOST: always on top of CS2.
        // WS_EX_TOOLWINDOW: don't show in the taskbar or alt-tab.
        hwnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            wc.lpszClassName, L"", WS_POPUP,
            0, 0, width_, height_, nullptr, nullptr, wc.hInstance, nullptr);
        if (!hwnd_) return false;

        // Fully transparent background, alpha-blended per pixel by what ImGui draws.
        SetLayeredWindowAttributes(hwnd_, RGB(0, 0, 0), 0, LWA_COLORKEY);

        if (!CreateDeviceAndSwapChain()) return false;

        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;  // don't write imgui.ini, we have no windows to persist
        ImGui_ImplWin32_Init(hwnd_);
        ImGui_ImplDX11_Init(device_.Get(), context_.Get());

        return true;
    }

    bool Overlay::CreateDeviceAndSwapChain() {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = width_;
        sd.BufferDesc.Height = height_;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd_;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        constexpr D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        const HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            levels, 1, D3D11_SDK_VERSION, &sd,
            swapChain_.GetAddressOf(), device_.GetAddressOf(), nullptr, context_.GetAddressOf());
        if (FAILED(hr)) return false;

        return CreateRenderTarget();
    }

    bool Overlay::CreateRenderTarget() {
        ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())))) return false;
        if (FAILED(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView_.GetAddressOf())))
            return false;
        return true;
    }

    void Overlay::CleanupDeviceAndSwapChain() {
        renderTargetView_.Reset();
    }

    void Overlay::HandleResize(UINT width, UINT height) {
        if (width == 0 || height == 0) return;  // minimized or degenerate, ignore
        CleanupDeviceAndSwapChain();
        const HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return;  // leaves renderTargetView_ null; next frame skips drawing, doesn't crash
        width_ = static_cast<int>(width);
        height_ = static_cast<int>(height);
        CreateRenderTarget();
    }

    void Overlay::Run(const OverlayDrawFn& drawFn, const std::function<bool()>& shouldStop) {
        MSG msg{};
        while (msg.message != WM_QUIT) {
            if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                continue;
            }
            if (shouldStop && shouldStop()) break;

            if (pendingResize_) {
                pendingResize_ = false;
                HandleResize(pendingWidth_, pendingHeight_);
            }
            if (!renderTargetView_) continue;  // mid-resize or device lost, skip this frame rather than crash

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            drawFn(width_, height_);

            ImGui::Render();
            const float clear[4] = { 0, 0, 0, 0 };  // fully transparent clear
            context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);
            context_->ClearRenderTargetView(renderTargetView_.Get(), clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            const HRESULT hr = swapChain_->Present(1, 0);
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
                // Device loss: tear down and rebuild rather than keep presenting into a dead device.
                ImGui_ImplDX11_InvalidateDeviceObjects();
                CleanupDeviceAndSwapChain();
                device_.Reset(); context_.Reset(); swapChain_.Reset();
                if (!CreateDeviceAndSwapChain()) break;  // can't recover, stop rather than spin
                ImGui_ImplDX11_CreateDeviceObjects();
            }
        }
    }

}  // namespace wrench