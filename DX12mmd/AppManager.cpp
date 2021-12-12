#include <d3dx12.h>
#include <vector>
#include <wrl/client.h>

#include "AppManager.hpp"
#include "Shader.hpp"
#include "Resource.hpp"


using Microsoft::WRL::ComPtr;

namespace
{
    GraphicEngine* s_Instance = nullptr;

    ResourceOrder s_MatrixResourceOrder =
    {
        "MatrixResource",
        { ResourceOrder::k_ConstantResource },
        1,
        0
    };
    ResourceOrder s_TextureResourceOrder =
    {
        "TextureResource",
        { ResourceOrder::k_ShaderResource },
        1,
        0
    };
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
    m_DepthBuffer(nullptr),
    m_DSV_Heap(nullptr),
    m_VertexShader(),
    m_PixelShader(),
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
#if 1
    // 透視変換行列設定
    auto world_mat = XMMatrixRotationY(XM_PI);

    XMFLOAT3 eye(0, 10, -15);
    XMFLOAT3 target(0, 10, 0);
    XMFLOAT3 up(0, 1, 0);

    auto view_mat = XMMatrixLookAtLH(
        XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up)
    );
    auto proj_mat = XMMatrixPerspectiveFovLH(
        XM_PIDIV2,
        static_cast<float>(k_WindowWidth) / static_cast<float>(k_WindowHeight),
        1.0f,
        100.0f
    );

    m_Matrix.World = world_mat;
    m_Matrix.ViewProj = view_mat* proj_mat;

    m_ConstBuff.Write(&m_Matrix, sizeof(m_Matrix));
#endif

    // バックバッファーのレンダーターゲットビューを、これから利用するレンダーターゲットビューに設定
    auto bbidx = m_Swapchain->GetCurrentBackBufferIndex();
    auto rtvH = m_RtvHeaps->GetCPUDescriptorHandleForHeapStart();
    rtvH.ptr += bbidx * m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // リソースバリア レンダーターゲットに設定
    SetRenderTargetResourceBarrier(bbidx, true);

    // パイプラインステートセット
    m_CmdList->SetPipelineState(m_PipelineState);

    auto dsv_desc_cpu_handle = m_DSV_Heap->GetCPUDescriptorHandleForHeapStart();
    m_CmdList->OMSetRenderTargets(1, &rtvH, true, &dsv_desc_cpu_handle);

    // レンダーターゲットクリア
    float clear_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_CmdList->ClearRenderTargetView(rtvH, clear_color, 0, nullptr);
    // Zバッファクリア
    // 1.0f = 最大値 でクリア
    m_CmdList->ClearDepthStencilView(dsv_desc_cpu_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    m_CmdList->SetGraphicsRootSignature(m_RootSignature);
    m_CmdList->RSSetViewports(1, &m_ViewPort);
    m_CmdList->RSSetScissorRects(1, &m_ScissorRect);

    m_CmdList->SetGraphicsRootSignature(m_RootSignature);
    auto descriptor_heap = m_Resource.DescriptorHeap();
    m_CmdList->SetDescriptorHeaps(1, &descriptor_heap);
    m_CmdList->SetGraphicsRootDescriptorTable(
        m_Resource.RootParameterID("TextureResource"),
        m_Textures.TextureDescriptorHeapGPU(m_TextureHandle)
    );

    auto handle = m_Resource.ResourceHandle("MatrixResource");
    m_CmdList->SetGraphicsRootDescriptorTable(
        m_Resource.RootParameterID("MatrixResource"),
        m_Resource.DescriptorHeapGPU(handle)
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
    if (!CreateDepthBuffer()) {
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

    std::vector<ResourceOrder> order;
    order.push_back(s_MatrixResourceOrder);
    order.push_back(s_TextureResourceOrder);
    if (!m_Resource.Initialize(order)) {
        return false;
    }

    auto texture_resource = m_Resource.ResourceHandle("TextureResource");
    if (!m_Textures.CreateTextures(&m_Resource, texture_resource, L"img/textest.png", &m_TextureHandle)) {
        return false;
    }

    m_Matrix.World = XMMatrixIdentity();
    m_Matrix.ViewProj = XMMatrixIdentity();
    if (!m_ConstBuff.Create(&m_Resource, sizeof(m_Matrix), m_Resource.ResourceHandle("MatrixResource"))) {
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

bool GraphicEngine::CreateDepthBuffer()
{
    D3D12_RESOURCE_DESC depth_res_desc{};

    depth_res_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 2次元のテクスチャデータ
    depth_res_desc.Width = k_WindowWidth;                           // 幅と高さはレンダーターゲットと同じ
    depth_res_desc.Height = k_WindowHeight;
    depth_res_desc.DepthOrArraySize = 1;                            // テクスチャ配列でも、3Dテクスチャでもない
    depth_res_desc.Format = DXGI_FORMAT_D32_FLOAT;                  // 深度値書き込み用フォーマット
    depth_res_desc.SampleDesc.Count = 1;
    depth_res_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // デプスステンシルとして利用

    auto depth_heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);


    D3D12_CLEAR_VALUE depth_clear_value{};
    depth_clear_value.DepthStencil.Depth = 1.0f;            // 深さ1.0f（最大値）でクリア
    depth_clear_value.Format = DXGI_FORMAT_D32_FLOAT;       // 32bit float値としてクリア

    auto result = m_Device->CreateCommittedResource(
        &depth_heap_prop,
        D3D12_HEAP_FLAG_NONE,
        &depth_res_desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,                   // 深度値書き込みに利用
        nullptr,
        IID_PPV_ARGS(&m_DepthBuffer)
    );

    if (result != S_OK) {
        return false;
    }

    // 深度のためのディスクリプタヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{};
    dsv_heap_desc.NumDescriptors = 1;                       // 深度ビューは１つ
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;    // デプスステンシルビューとして使う
    result = m_Device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&m_DSV_Heap));
    if (result != S_OK) {
        return false;
    }

    // 深度ビュー作成
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
    dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;                    // 深度値に32ビット使用
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;     // 2Dテクスチャ
    dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

    m_Device->CreateDepthStencilView(
        m_DepthBuffer,
        &dsv_desc,
        m_DSV_Heap->GetCPUDescriptorHandleForHeapStart()
    );

    return true;
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

    // Zバッファ設定
    gpipeline.DepthStencilState.DepthEnable = true;
    gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;    // すべて書き込む
    gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;         // 小さいほうを採用
    gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;                                // 深度値は 32bit float

    // 入力レイアウトの設定
    gpipeline.InputLayout.pInputElementDescs = m_VertBuff->GetVertexLayout();
    gpipeline.InputLayout.NumElements = m_VertBuff->VertexLayoutLength();

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

