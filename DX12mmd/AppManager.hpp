#pragma once


#include <d3d12.h>
#include <dxgi1_6.h>

#include "Fence.hpp"
#include "Shader.hpp"
#include "VertexBuffer.hpp"
#include "IndexBuffer.hpp"
#include "ConstantBuffer.hpp"
#include "Texture.hpp"
#include "Resource.hpp"
#include "Matrix.hpp"
#include "PMDActor.hpp"

class GraphicEngine
{
public:
    static constexpr int k_WindowWidth = 1152;
    static constexpr int k_WindowHeight = 648;


    static bool Initialize(HWND hwnd);
    static GraphicEngine& Instance();
    static void EnableDebugLayer();

    bool CreateGraphicPipeLine();
    ID3D12Device* Device(); 
    ID3D12GraphicsCommandList* CmdList();
    ResourceManager* Resource();
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
    bool CreateDepthBuffer();
    bool LinkSwapchainToDesc();
    bool CreateRootSignature(ID3D12RootSignature** rootsignature);
    bool SetRenderTargetResourceBarrier(UINT bbidx, bool barrier_on_flag);

    ID3D12Device* m_Device;
    IDXGIFactory6* m_DxgiFactory;
    IDXGISwapChain4* m_Swapchain;

    ID3D12CommandAllocator* m_CmdAllocator;
    ID3D12GraphicsCommandList* m_CmdList;

    ID3D12CommandQueue* m_CmdQueue;
    ID3D12DescriptorHeap* m_RtvHeaps;

    ID3D12Resource* m_DepthBuffer;
    ID3D12DescriptorHeap* m_DSV_Heap;

    CompileShader m_VertexShader;
    CompileShader m_PixelShader;

    ID3D12PipelineState* m_PipelineState;
    ID3D12RootSignature* m_RootSignature;

    Fence m_Fence;

    D3D12_VIEWPORT m_ViewPort;
    D3D12_RECT m_ScissorRect;
    ConstantBuffer m_ConstBuff;

    SceneMatrix m_Matrix;
    PMDActor m_Model;

    ResourceManager m_Resource;
    TextureGroup m_Textures;
};