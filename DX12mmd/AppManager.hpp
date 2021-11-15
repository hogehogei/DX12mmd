#pragma once


#include <d3d12.h>
#include <dxgi1_6.h>

#include "Fence.hpp"
#include "Shader.hpp"
#include "VertexBuffer.hpp"
#include "IndexBuffer.hpp"
#include "Texture.hpp"

class GraphicEngine
{
public:
    static constexpr int k_WindowWidth = 1152;
    static constexpr int k_WindowHeight = 648;


    static bool Initialize(HWND hwnd);
    static GraphicEngine& Instance();
    static void EnableDebugLayer();

    ID3D12Device* Device(); 
    ID3D12GraphicsCommandList* CmdList();
    void ExecCmdQueue();

    void SetViewPort();
    void SetVertexBuffer(VertexBufferPtr vertbuff);
    void SetIndexBuffer(IndexBufferPtr idxbuff);
    void FlipWindow();

private:

    GraphicEngine();

    bool InitializeDX12(HWND hwnd);
    bool InitializeCommandQueue();
    bool CreateSwapChain(HWND hwnd);
    bool CreateDescriptorHeap();
    bool LinkSwapchainToDesc();
    bool CreateGraphicPipeLine();
    bool CreateRootSignature(ID3D12RootSignature** rootsignature);
    bool SetRenderTargetResourceBarrier(UINT bbidx, bool barrier_on_flag);
    void SetVertexLayout();

    ID3D12Device* m_Device;
    IDXGIFactory6* m_DxgiFactory;
    IDXGISwapChain4* m_Swapchain;

    ID3D12CommandAllocator* m_CmdAllocator;
    ID3D12GraphicsCommandList* m_CmdList;

    ID3D12CommandQueue* m_CmdQueue;
    ID3D12DescriptorHeap* m_RtvHeaps;

    CompileShader m_VertexShader;
    CompileShader m_PixelShader;
    D3D12_INPUT_ELEMENT_DESC m_InputLayout[2];

    ID3D12PipelineState* m_PipelineState;
    ID3D12RootSignature* m_RootSignature;

    Fence m_Fence;

    D3D12_VIEWPORT m_ViewPort;
    D3D12_RECT m_ScissorRect;
    VertexBufferPtr m_VertBuff;
    IndexBufferPtr m_IdxBuff;

    TextureGroup m_Textures;
};