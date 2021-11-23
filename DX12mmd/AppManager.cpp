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
    // コマンドリスト実行
    ID3D12CommandList* cmdlists[] = { m_CmdList };
    m_CmdQueue->ExecuteCommandLists(1, cmdlists);

    // コマンド実行待ち
    m_Fence.WaitCmdComplete();
}

void GraphicEngine::SetViewPort()
{
    m_ViewPort.Width = k_WindowWidth;       // 出力先の幅
    m_ViewPort.Height = k_WindowHeight;     // 出力先の高さ
    m_ViewPort.TopLeftX = 0;                // 出力先の左上座標X
    m_ViewPort.TopLeftY = 0;                // 出力先の左上座標Y
    m_ViewPort.MaxDepth = 1.0f;             // 深度最大値
    m_ViewPort.MinDepth = 0.0f;             // 深度最小値

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
    // バックバッファーのレンダーターゲットビューを、これから利用するレンダーターゲットビューに設定
    auto bbidx = m_Swapchain->GetCurrentBackBufferIndex();
    auto rtvH = m_RtvHeaps->GetCPUDescriptorHandleForHeapStart();
    rtvH.ptr += bbidx * m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // リソースバリア レンダーターゲットに設定
    SetRenderTargetResourceBarrier(bbidx, true);

    // パイプラインステートセット
    m_CmdList->SetPipelineState(m_PipelineState);

    m_CmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

    // レンダーターゲットクリア
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

    // 頂点バッファービュー設定
    auto vbview = m_VertBuff->GetVertexBufferView();
    m_CmdList->IASetVertexBuffers(0, 1, &vbview);
    auto idxview = m_IdxBuff->GetIndexBufferView();
    m_CmdList->IASetIndexBuffer(&idxview);
    //描写命令
    m_CmdList->DrawIndexedInstanced(m_IdxBuff->Count(), 1, 0, 0, 0);

    // リソースバリアをPRESENTに戻す
    SetRenderTargetResourceBarrier(bbidx, false);

    // 命令クローズ
    m_CmdList->Close();

    // コマンドリスト実行
    ID3D12CommandList* cmdlists[] = { m_CmdList };
    m_CmdQueue->ExecuteCommandLists(1, cmdlists);

    // コマンド実行待ち
    m_Fence.WaitCmdComplete();

    // キューをクリア
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

    // タイムアウトなし
    cmd_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    // アダプターを一つしか使わないときは0でよい
    cmd_queue_desc.NodeMask = 0;
    // プライオリティは特に指定なし
    cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    // コマンドリストと合わせる
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
    swapchain_desc.SampleDesc.Count = 1;        // AAを使用しないので1
    swapchain_desc.SampleDesc.Quality = 0;      // AAを使用しないので0
    swapchain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    swapchain_desc.BufferCount = 2;

    // バックバッファーは伸び縮み可能
    swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
    // フリップ後は速やかに破棄
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    // 特に指定なし
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // ウィンドウ⇔フルスクリーン切り替え可能
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

    // レンダーターゲットビューなのでRTV
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

    // SRGBレンダーターゲットビュー設定
    D3D12_RENDER_TARGET_VIEW_DESC rtvdesc{};
    rtvdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RtvHeaps->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < backbuffers.size(); ++i) {
        // レンダーターゲットビューを生成する
        m_Device->CreateRenderTargetView(backbuffers[i], &rtvdesc, handle);
        // ポインターをずらす
        handle.ptr += m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    return true;
}

bool GraphicEngine::CreateGraphicPipeLine()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};

    gpipeline.pRootSignature = nullptr;     // あとで設定する
    gpipeline.VS.pShaderBytecode = m_VertexShader.GetBlob()->GetBufferPointer();
    gpipeline.VS.BytecodeLength = m_VertexShader.GetBlob()->GetBufferSize();
    gpipeline.PS.pShaderBytecode = m_PixelShader.GetBlob()->GetBufferPointer();
    gpipeline.PS.BytecodeLength = m_PixelShader.GetBlob()->GetBufferSize();

    // デフォルトのサンプルマスクを表す定数
    gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    
    // まだアンチエイリアスは使わないため false
    gpipeline.RasterizerState.MultisampleEnable = false;

    gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;      // カリングしない
    gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;     // 中身を塗りつぶす
    gpipeline.RasterizerState.DepthClipEnable = true;               // 深度方向のクリッピングは有効

    // ブレンドステートの設定
    // アンチエイリアス、ブレンドを使用しない設定
    gpipeline.BlendState.AlphaToCoverageEnable = false;
    gpipeline.BlendState.IndependentBlendEnable = false;

    D3D12_RENDER_TARGET_BLEND_DESC render_target_blend_desc{};
    render_target_blend_desc.BlendEnable = false;
    render_target_blend_desc.LogicOpEnable = false;
    render_target_blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    gpipeline.BlendState.RenderTarget[0] = render_target_blend_desc;

    // 入力レイアウトの設定
    SetVertexLayout();
    gpipeline.InputLayout.pInputElementDescs = m_InputLayout;
    gpipeline.InputLayout.NumElements = 2;

    // トライアングルカットなし
    gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    // 三角形で構成
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    
    // レンダーターゲット設定
    gpipeline.NumRenderTargets = 1;
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    // アンチエイリアシングのサンプル数設定
    gpipeline.SampleDesc.Count = 1;         // サンプリングは1ピクセルにつき1
    gpipeline.SampleDesc.Quality = 0;       // クォリティは最低

    // ルートシグネチャ設定
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
        0,              // nodemask 0 でよい。
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

    barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 遷移
    barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;      // 特に指定なし
    barrier_desc.Transition.pResource = backbuffer;             // バックバッファーリソース
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
    // 座標情報
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

