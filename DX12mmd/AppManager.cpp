#include "AppManager.hpp"
#include "Shader.hpp"
#include "Resource.hpp"

#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
    GraphicEngine* s_Instance = nullptr;
}

GraphicEngine::GraphicEngine()
    :
    m_Device(nullptr),
    m_DxgiFactory(nullptr),
    m_Swapchain(nullptr),
    m_CmdAllocator(nullptr),
    m_CmdList(nullptr),
    m_CmdQueue(nullptr),
    m_RtvHeaps(nullptr),
    m_VertexShader(),
    m_PixelShader(),
    m_InputLayout(),
    m_PipelineState(nullptr),
    m_RootSignature(nullptr),
    m_Fence(),
    m_ViewPort(),
    m_ScissorRect(),
    m_VertBuff(),
    m_IdxBuff(),
    m_ConstBuff(),
    m_Resource(),
    m_Textures(),
    m_TextureHandle(0)
{}

bool GraphicEngine::Initialize( HWND hwnd )
{
    if (s_Instance == nullptr) {
        s_Instance = new GraphicEngine();
    }

    if (!s_Instance->InitializeDX12( hwnd )) {
        return false;
    }

    return true;
}

GraphicEngine& GraphicEngine::Instance()
{
    return *s_Instance;
}

void GraphicEngine::EnableDebugLayer()
{
    ID3D12Debug* debug_layer = nullptr;
    auto result = D3D12GetDebugInterface( IID_PPV_ARGS(&debug_layer) );
    
    debug_layer->EnableDebugLayer();
    debug_layer->Release();
}

ID3D12Device* GraphicEngine::Device()
{
    return m_Device;
}

ID3D12GraphicsCommandList* GraphicEngine::CmdList()
{
    return m_CmdList;
}

void GraphicEngine::ExecCmdQueue()
{
    // �R�}���h���X�g���s
    ID3D12CommandList* cmdlists[] = { m_CmdList };
    m_CmdQueue->ExecuteCommandLists(1, cmdlists);

    // �R�}���h���s�҂�
    m_Fence.WaitCmdComplete();
}

void GraphicEngine::SetViewPort()
{
    m_ViewPort.Width = k_WindowWidth;       // �o�͐�̕�
    m_ViewPort.Height = k_WindowHeight;     // �o�͐�̍���
    m_ViewPort.TopLeftX = 0;                // �o�͐�̍�����WX
    m_ViewPort.TopLeftY = 0;                // �o�͐�̍�����WY
    m_ViewPort.MaxDepth = 1.0f;             // �[�x�ő�l
    m_ViewPort.MinDepth = 0.0f;             // �[�x�ŏ��l

    m_ScissorRect.top = 0;
    m_ScissorRect.left = 0;
    m_ScissorRect.right = m_ScissorRect.left + k_WindowWidth;
    m_ScissorRect.bottom = m_ScissorRect.top + k_WindowHeight;
}

void GraphicEngine::SetVertexBuffer(VertexBufferPtr vertbuff)
{
    m_VertBuff = vertbuff;
}

void GraphicEngine::SetIndexBuffer(IndexBufferPtr idxbuff)
{
    m_IdxBuff = idxbuff;
}

void GraphicEngine::FlipWindow()
{
    // �o�b�N�o�b�t�@�[�̃����_�[�^�[�Q�b�g�r���[���A���ꂩ�痘�p���郌���_�[�^�[�Q�b�g�r���[�ɐݒ�
    auto bbidx = m_Swapchain->GetCurrentBackBufferIndex();
    auto rtvH = m_RtvHeaps->GetCPUDescriptorHandleForHeapStart();
    rtvH.ptr += bbidx * m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // ���\�[�X�o���A �����_�[�^�[�Q�b�g�ɐݒ�
    SetRenderTargetResourceBarrier(bbidx, true);

    // �p�C�v���C���X�e�[�g�Z�b�g
    m_CmdList->SetPipelineState(m_PipelineState);

    m_CmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

    // �����_�[�^�[�Q�b�g�N���A
    float clear_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_CmdList->ClearRenderTargetView(rtvH, clear_color, 0, nullptr);
    m_CmdList->SetGraphicsRootSignature(m_RootSignature);
    m_CmdList->RSSetViewports(1, &m_ViewPort);
    m_CmdList->RSSetScissorRects(1, &m_ScissorRect);

    m_CmdList->SetGraphicsRootSignature(m_RootSignature);
    auto descriptor_heap = m_Resource.DescriptorHeap();
    m_CmdList->SetDescriptorHeaps(1, &descriptor_heap);
    m_CmdList->SetGraphicsRootDescriptorTable(
        m_Resource.TextureRootParameterID(),
        m_Textures.TextureDescriptorHeapGPU(m_TextureHandle)
    );
    m_CmdList->SetGraphicsRootDescriptorTable(
        m_Resource.ConstantRootParameterID(),
        m_Resource.ConstantDescriptorHeapGPU(0)
    );
    
    m_CmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ���_�o�b�t�@�[�r���[�ݒ�
    auto vbview = m_VertBuff->GetVertexBufferView();
    m_CmdList->IASetVertexBuffers(0, 1, &vbview);
    auto idxview = m_IdxBuff->GetIndexBufferView();
    m_CmdList->IASetIndexBuffer(&idxview);
    //�`�ʖ���
    m_CmdList->DrawIndexedInstanced(m_IdxBuff->Count(), 1, 0, 0, 0);

    // ���\�[�X�o���A��PRESENT�ɖ߂�
    SetRenderTargetResourceBarrier(bbidx, false);

    // ���߃N���[�Y
    m_CmdList->Close();

    // �R�}���h���X�g���s
    ID3D12CommandList* cmdlists[] = { m_CmdList };
    m_CmdQueue->ExecuteCommandLists(1, cmdlists);

    // �R�}���h���s�҂�
    m_Fence.WaitCmdComplete();

    // �L���[���N���A
    m_CmdAllocator->Reset();
    m_CmdList->Reset(m_CmdAllocator, nullptr);

    m_Swapchain->Present(1, 0);
}

bool GraphicEngine::InitializeDX12( HWND hwnd )
{
    if (D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_Device)) != S_OK) {
        return false;
    }

#ifdef _DEBUG
    if ( CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_DxgiFactory)) != S_OK ) {
#else
    if (CreateDXGIFactory1(IID_PPV_ARGS(&m_DxgiFactory)) != S_OK) {
#endif
        return false;
    }

    auto result = m_Device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_CmdAllocator)
    );
    if (result != S_OK) {
        return false;
    }

    result = m_Device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_CmdAllocator,
        nullptr,
        IID_PPV_ARGS(&m_CmdList)
    );
    if (result != S_OK) {
        return false;
    }

    if (!InitializeCommandQueue()) {
        return false;
    }
    if (!CreateSwapChain(hwnd)) {
        return false;
    }
    if (!CreateDescriptorHeap()) {
        return false;
    }
    if (!LinkSwapchainToDesc()) {
        return false;
    }

    if (!m_Fence.Initialize(m_Device, m_CmdQueue)) {
        return false;
    }

    m_VertexShader = CompileShader(L"BasicVertexShader.hlsl", "BasicVS", CompileShader::Type::k_VertexShader);
    m_PixelShader = CompileShader(L"BasicPixelShader.hlsl", "BasicPS", CompileShader::Type::k_PixelShader);
    if (!m_VertexShader.IsValid() || !m_PixelShader.IsValid()) {
        return false;
    }

    if (!m_Resource.Initialize(1, 1)) {
        return false;
    }

    if (!m_Textures.CreateTextures(&m_Resource, L"img/textest.png", &m_TextureHandle)) {
        return false;
    }

    m_Matrix = XMMatrixIdentity();
    if (!m_ConstBuff.Create(&m_Resource, sizeof(XMMATRIX), 0)) {
        return false;
    }
    m_Matrix.r[0].m128_f32[0] = 2.0f / k_WindowWidth;
    m_Matrix.r[1].m128_f32[1] = -2.0f / k_WindowHeight;
    m_Matrix.r[3].m128_f32[0] = -1.0f;
    m_Matrix.r[3].m128_f32[1] = 1.0f;
    m_ConstBuff.Write(&m_Matrix, sizeof(m_Matrix));

    if (!CreateGraphicPipeLine()) {
        return false;
    }

    return true;
}

bool GraphicEngine::InitializeCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC cmd_queue_desc{};

    // �^�C���A�E�g�Ȃ�
    cmd_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    // �A�_�v�^�[��������g��Ȃ��Ƃ���0�ł悢
    cmd_queue_desc.NodeMask = 0;
    // �v���C�I���e�B�͓��Ɏw��Ȃ�
    cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    // �R�}���h���X�g�ƍ��킹��
    cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    auto result = m_Device->CreateCommandQueue( &cmd_queue_desc, IID_PPV_ARGS(&m_CmdQueue) );
    
    return result == S_OK;
}

bool GraphicEngine::CreateSwapChain( HWND hwnd )
{
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};

    swapchain_desc.Width = k_WindowWidth;
    swapchain_desc.Height = k_WindowHeight;
    swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.Stereo = false;
    swapchain_desc.SampleDesc.Count = 1;        // AA���g�p���Ȃ��̂�1
    swapchain_desc.SampleDesc.Quality = 0;      // AA���g�p���Ȃ��̂�0
    swapchain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    swapchain_desc.BufferCount = 2;

    // �o�b�N�o�b�t�@�[�͐L�яk�݉\
    swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
    // �t���b�v��͑��₩�ɔj��
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    // ���Ɏw��Ȃ�
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // �E�B���h�E�̃t���X�N���[���؂�ւ��\
    swapchain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    auto result = m_DxgiFactory->CreateSwapChainForHwnd(
        m_CmdQueue,
        hwnd,
        &swapchain_desc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(&m_Swapchain)
    );

    return result == S_OK;
}

bool GraphicEngine::CreateDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapdesc{};

    // �����_�[�^�[�Q�b�g�r���[�Ȃ̂�RTV
    heapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapdesc.NodeMask = 0;
    heapdesc.NumDescriptors = 2;
    heapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    auto result = m_Device->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(&m_RtvHeaps));

    return result == S_OK;
}

bool GraphicEngine::LinkSwapchainToDesc()
{
    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};

    auto result = m_Swapchain->GetDesc(&swap_chain_desc);
    if (result != S_OK) {
        return false;
    }

    std::vector<ID3D12Resource*> backbuffers(swap_chain_desc.BufferCount);
    for (int i = 0; i < backbuffers.size(); ++i){
        m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffers[i]));
    }

    // SRGB�����_�[�^�[�Q�b�g�r���[�ݒ�
    D3D12_RENDER_TARGET_VIEW_DESC rtvdesc{};
    rtvdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RtvHeaps->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < backbuffers.size(); ++i) {
        // �����_�[�^�[�Q�b�g�r���[�𐶐�����
        m_Device->CreateRenderTargetView(backbuffers[i], &rtvdesc, handle);
        // �|�C���^�[�����炷
        handle.ptr += m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    return true;
}

bool GraphicEngine::CreateGraphicPipeLine()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};

    gpipeline.pRootSignature = nullptr;     // ���ƂŐݒ肷��
    gpipeline.VS.pShaderBytecode = m_VertexShader.GetBlob()->GetBufferPointer();
    gpipeline.VS.BytecodeLength = m_VertexShader.GetBlob()->GetBufferSize();
    gpipeline.PS.pShaderBytecode = m_PixelShader.GetBlob()->GetBufferPointer();
    gpipeline.PS.BytecodeLength = m_PixelShader.GetBlob()->GetBufferSize();

    // �f�t�H���g�̃T���v���}�X�N��\���萔
    gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    
    // �܂��A���`�G�C���A�X�͎g��Ȃ����� false
    gpipeline.RasterizerState.MultisampleEnable = false;

    gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;      // �J�����O���Ȃ�
    gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;     // ���g��h��Ԃ�
    gpipeline.RasterizerState.DepthClipEnable = true;               // �[�x�����̃N���b�s���O�͗L��

    // �u�����h�X�e�[�g�̐ݒ�
    // �A���`�G�C���A�X�A�u�����h���g�p���Ȃ��ݒ�
    gpipeline.BlendState.AlphaToCoverageEnable = false;
    gpipeline.BlendState.IndependentBlendEnable = false;

    D3D12_RENDER_TARGET_BLEND_DESC render_target_blend_desc{};
    render_target_blend_desc.BlendEnable = false;
    render_target_blend_desc.LogicOpEnable = false;
    render_target_blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    gpipeline.BlendState.RenderTarget[0] = render_target_blend_desc;

    // ���̓��C�A�E�g�̐ݒ�
    SetVertexLayout();
    gpipeline.InputLayout.pInputElementDescs = m_InputLayout;
    gpipeline.InputLayout.NumElements = 2;

    // �g���C�A���O���J�b�g�Ȃ�
    gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    // �O�p�`�ō\��
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    
    // �����_�[�^�[�Q�b�g�ݒ�
    gpipeline.NumRenderTargets = 1;
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    // �A���`�G�C���A�V���O�̃T���v�����ݒ�
    gpipeline.SampleDesc.Count = 1;         // �T���v�����O��1�s�N�Z���ɂ�1
    gpipeline.SampleDesc.Quality = 0;       // �N�H���e�B�͍Œ�

    // ���[�g�V�O�l�`���ݒ�
    ID3D12RootSignature* rootsignature = nullptr;
    if (!CreateRootSignature(&rootsignature)) {
        return false;
    }
    gpipeline.pRootSignature = rootsignature;
    m_RootSignature = rootsignature;

    auto result = m_Device->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&m_PipelineState));
    if (result != S_OK) {
        return false;
    }

    return true;
}

bool GraphicEngine::CreateRootSignature( ID3D12RootSignature** rootsignature )
{
    D3D12_ROOT_SIGNATURE_DESC rootsig_desc{};
    rootsig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    rootsig_desc.pParameters = m_Resource.RootParameter();
    rootsig_desc.NumParameters = m_Resource.RootParameterSize();

    rootsig_desc.pStaticSamplers = m_Resource.SamplerDescriptor();
    rootsig_desc.NumStaticSamplers = 1;

    ComPtr<ID3DBlob> rootsig_blob = nullptr;
    ComPtr<ID3DBlob> error_blob = nullptr;
    auto result = D3D12SerializeRootSignature(
        &rootsig_desc,
        D3D_ROOT_SIGNATURE_VERSION_1_0,
        &rootsig_blob,
        &error_blob
    );

    if (result != S_OK) {
        return false;
    }

    result = m_Device->CreateRootSignature(
        0,              // nodemask 0 �ł悢�B
        rootsig_blob->GetBufferPointer(),
        rootsig_blob->GetBufferSize(),
        IID_PPV_ARGS(rootsignature)
    );

    return result == S_OK;
}

bool GraphicEngine::SetRenderTargetResourceBarrier( UINT bbidx, bool barrier_on_flag )
{
    ID3D12Resource* backbuffer = nullptr;
    if (m_Swapchain->GetBuffer(bbidx, IID_PPV_ARGS(&backbuffer)) != S_OK) {
        return false;
    }

    D3D12_RESOURCE_BARRIER barrier_desc{};

    barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // �J��
    barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;      // ���Ɏw��Ȃ�
    barrier_desc.Transition.pResource = backbuffer;             // �o�b�N�o�b�t�@�[���\�[�X
    barrier_desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    if (barrier_on_flag) {
        barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier_desc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    else {
        barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier_desc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    }

    m_CmdList->ResourceBarrier(1, &barrier_desc);
    
    return true;
}


void GraphicEngine::SetVertexLayout()
{
    // ���W���
    m_InputLayout[0] = {
        "POSITION",
        0,
        DXGI_FORMAT_R32G32B32_FLOAT,
        0,
        D3D12_APPEND_ALIGNED_ELEMENT,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0
    };
    // uv
    m_InputLayout[1] = {
        "TEXCOORD",
        0,
        DXGI_FORMAT_R32G32_FLOAT,
        0,
        D3D12_APPEND_ALIGNED_ELEMENT,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0
    };
}

