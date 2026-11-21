#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <functional>

namespace wrench {

    using Microsoft::WRL::ComPtr;

    // Draw callback receives the ImGui frame already begun; it should issue ImGui draw calls
    // (ImGui::GetBackgroundDrawList(), etc.) and return. Overlay handles everything else:
    // window, device, resize, device-loss, and shutdown.
    using OverlayDrawFn = std::function<void(int width, int height)>;

    class Overlay {
    public:
        Overlay() = default;
        Overlay(const Overlay&) = delete;
        Overlay& operator=(const Overlay&) = delete;
        ~Overlay();

        // Creates the click-through topmost window and the D3D11 device. Returns false on failure.
        bool Init();

        // Pumps messages and renders until the window is closed or shouldStop returns true.
        // drawFn is called once per frame between ImGui::NewFrame() and ImGui::Render().
        void Run(const OverlayDrawFn& drawFn, const std::function<bool()>& shouldStop);

    private:
        bool CreateDeviceAndSwapChain();
        void CleanupDeviceAndSwapChain();   // releases RTV + swapchain-dependent state only
        bool CreateRenderTarget();
        void HandleResize(UINT width, UINT height);

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

        HWND hwnd_ = nullptr;
        ComPtr<ID3D11Device> device_;
        ComPtr<ID3D11DeviceContext> context_;
        ComPtr<IDXGISwapChain> swapChain_;
        ComPtr<ID3D11RenderTargetView> renderTargetView_;
        int width_ = 0;
        int height_ = 0;
        bool pendingResize_ = false;
        UINT pendingWidth_ = 0, pendingHeight_ = 0;
    };

}  // namespace wrench