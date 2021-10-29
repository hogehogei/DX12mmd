#pragma once


#include <d3d12.h>
#include <dxgi1_6.h>

#include "Fence.hpp"

class GraphicEngine
{
public:
    static constexpr int k_WindowWidth = 1152;
    static constexpr int k_WindowHeight = 648;


    static bool Initialize(HWND hwnd);
    static GraphicEngine& Instance();
    static void EnableDebugLayer();

    void FlipWindow();

private:

    GraphicEngine();

    bool InitializeDX12(HWND hwnd);
    bool InitializeCommandQueue();
    bool CreateSwapChain(HWND hwnd);
    bool CreateDescriptorHeap();
    bool LinkSwapchainToDesc();
    bool SetRenderTargetResourceBarrier(UINT bbidx, bool barrier_on_flag);

    ID3D12Device* m_Device;
    IDXGIFactory6* m_DxgiFactory;
    IDXGISwapChain4* m_Swapchain;

    ID3D12CommandAllocator* m_CmdAllocator;
    ID3D12GraphicsCommandList* m_CmdList;

    ID3D12CommandQueue* m_CmdQueue;

    ID3D12DescriptorHeap* m_RtvHeaps;

    Fence m_Fence;
};