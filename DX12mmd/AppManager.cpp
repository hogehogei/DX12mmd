
// for Windows problem that std::min conflict
#define NOMINMAX

#include <d3dx12.h>
#include <algorithm>
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
        { {ResourceOrder::k_ConstantResource, 1, 0} },
        1,
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
    m_ConstBuff(),
    m_Model(),
    m_Resource(),
    m_Textures()
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

ResourceManager* GraphicEngine::Resource()
{
    return &m_Resource;
}

void GraphicEngine::ExecCmdQueue()
{
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

void GraphicEngine::FlipWindow()
{
#if 1
    // �����ϊ��s��ݒ�
    auto world_mat = XMMatrixRotationY(0);

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
    m_Matrix.View = view_mat;
    m_Matrix.Proj = proj_mat;
    m_Matrix.Eye = eye;

    m_ConstBuff.Write(&m_Matrix, sizeof(m_Matrix));

#endif

    // ���f���`�ʏ���
    m_Model.MotionUpdate();
    // �A�j���[�V�����ϊ���̃{�[���ϊ��s����V�F�[�_�[�ɓn��
    auto& bone_metrices = m_Model.GetBoneMetricesForMotion();
    std::copy_n(
        bone_metrices.begin(),
        std::min<size_t>(bone_metrices.size(), k_BoneMetricesNum),
        &m_Matrix.Bones[0]
    );

    // �o�b�N�o�b�t�@�[�̃����_�[�^�[�Q�b�g�r���[���A���ꂩ�痘�p���郌���_�[�^�[�Q�b�g�r���[�ɐݒ�
    auto bbidx = m_Swapchain->GetCurrentBackBufferIndex();
    auto rtvH = m_RtvHeaps->GetCPUDescriptorHandleForHeapStart();
    rtvH.ptr += bbidx * m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // ���\�[�X�o���A �����_�[�^�[�Q�b�g�ɐݒ�
    SetRenderTargetResourceBarrier(bbidx, true);

    // �p�C�v���C���X�e�[�g�Z�b�g
    m_CmdList->SetPipelineState(m_PipelineState);

    auto dsv_desc_cpu_handle = m_DSV_Heap->GetCPUDescriptorHandleForHeapStart();
    m_CmdList->OMSetRenderTargets(1, &rtvH, true, &dsv_desc_cpu_handle);

    // �����_�[�^�[�Q�b�g�N���A
    float clear_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_CmdList->ClearRenderTargetView(rtvH, clear_color, 0, nullptr);
    // Z�o�b�t�@�N���A
    // 1.0f = �ő�l �ŃN���A
    m_CmdList->ClearDepthStencilView(dsv_desc_cpu_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    m_CmdList->SetGraphicsRootSignature(m_RootSignature);
    m_CmdList->RSSetViewports(1, &m_ViewPort);
    m_CmdList->RSSetScissorRects(1, &m_ScissorRect);

    auto handle = m_Resource.ResourceHandle("MatrixResource");
    auto descriptor_heap = m_Resource.DescriptorHeap(handle);
    m_CmdList->SetDescriptorHeaps(1, &descriptor_heap);
    m_CmdList->SetGraphicsRootDescriptorTable(
        m_Resource.RootParameterID("MatrixResource"),
        m_Resource.DescriptorHeapGPU(handle)
    );
    
    m_CmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ���_�o�b�t�@�[�r���[�ݒ�
    auto vbview = m_Model.GetVertexBuffer()->GetVertexBufferView();
    m_CmdList->IASetVertexBuffers(0, 1, &vbview);
    auto idxview = m_Model.GetIndexBuffer()->GetIndexBufferView();
    m_CmdList->IASetIndexBuffer(&idxview);
    
    //�`�ʖ���
    handle = m_Resource.ResourceHandle("Miku");
    descriptor_heap = m_Resource.DescriptorHeap(handle);
    m_CmdList->SetDescriptorHeaps(1, &descriptor_heap);

    //auto materialH = m_Model.DescriptorHeapGPU();
    const std::vector<Material>& materials = m_Model.GetPMDData().MaterialData();
    unsigned int idx_offset = 0;
    
    for (const auto& m : materials) {
        m_CmdList->SetGraphicsRootDescriptorTable(
            m_Resource.RootParameterID("Miku"),
            m_Resource.DescriptorHeapGPU(handle)
        );
        m_CmdList->DrawIndexedInstanced(m.IndicesNum, 1, idx_offset, 0, 0);
        handle.Advance();
        idx_offset += m.IndicesNum;
    }

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
    //order.push_back(s_TextureResourceOrder);
    if (!m_Resource.Initialize(order)) {
        return false;
    }

    //auto texture_resource = m_Resource.ResourceHandle("TextureResource");
    //if (!m_Textures.CreateTextures(&m_Resource, texture_resource, L"img/textest.png", &m_TextureHandle)) {
    //    return false;
    //}

    if (!m_Model.Create(&m_Resource, "Miku", "Model/�����~�N.pmd", "Model/motion.vmd")) {
        return false;
    }

    m_Matrix.World = XMMatrixIdentity();
    m_Matrix.View = XMMatrixIdentity();
    m_Matrix.Proj = XMMatrixIdentity();
    if (!m_ConstBuff.Create(&m_Resource, sizeof(m_Matrix), 1, m_Resource.ResourceHandle("MatrixResource"))) {
        return false;
    }

    // �A�j���[�V�����X�^�[�g
    m_Model.PlayAnimation();

#if 0
    // ���[�V�����e�X�g
    // �{�[�������R�s�[
    auto& bone_metrices = m_Model.GetBoneMetricesForMotion();

    const auto& vmd_motion_table = m_Model.GetVMDMotionTable().GetMotionTable();
    for (const auto& bone_motion : vmd_motion_table) {
        const auto& node = m_Model.GetPMDData().BoneFromName(bone_motion.first);
        const auto& pos = node->BoneStartPos;

        auto mat =
            DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z) *
            DirectX::XMMatrixRotationQuaternion(bone_motion.second[0].Quaternion) *
            DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

        bone_metrices[node->BoneIdx] = mat;
    }
#endif
#if 0
    // �����ɘr����]
    auto arm_bone = m_Model.GetPMDData().BoneFromName("���r");
    if (arm_bone) {
        const auto& pos = arm_bone.value().BoneStartPos;
        auto mat =
            DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z) *
            XMMatrixRotationZ(DirectX::XM_PIDIV2) *
            DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

        bone_metrices[arm_bone.value().BoneIdx] = mat;
    }
    // �����ɂЂ�����]
    auto elbow_bone = m_Model.GetPMDData().BoneFromName("���Ђ�");
    if (elbow_bone) {
        const auto& pos = elbow_bone.value().BoneStartPos;
        auto mat =
            DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z) *
            XMMatrixRotationZ(-DirectX::XM_PIDIV2) *
            DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

        bone_metrices[elbow_bone.value().BoneIdx] = mat;
    }

    auto root_bone = m_Model.GetPMDData().BoneFromName("�Z���^�[");
    if (root_bone) {
        m_Model.RecursiveMatrixMultiply(&(root_bone.value()), DirectX::XMMatrixIdentity());
    }

    std::copy_n(
        bone_metrices.begin(),
        std::min<size_t>(bone_metrices.size(), k_BoneMetricesNum),
        &m_Matrix.Bones[0]
    );
#endif

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

bool GraphicEngine::CreateDepthBuffer()
{
    D3D12_RESOURCE_DESC depth_res_desc{};

    depth_res_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 2�����̃e�N�X�`���f�[�^
    depth_res_desc.Width = k_WindowWidth;                           // ���ƍ����̓����_�[�^�[�Q�b�g�Ɠ���
    depth_res_desc.Height = k_WindowHeight;
    depth_res_desc.DepthOrArraySize = 1;                            // �e�N�X�`���z��ł��A3D�e�N�X�`���ł��Ȃ�
    depth_res_desc.Format = DXGI_FORMAT_D32_FLOAT;                  // �[�x�l�������ݗp�t�H�[�}�b�g
    depth_res_desc.SampleDesc.Count = 1;
    depth_res_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // �f�v�X�X�e���V���Ƃ��ė��p

    auto depth_heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);


    D3D12_CLEAR_VALUE depth_clear_value{};
    depth_clear_value.DepthStencil.Depth = 1.0f;            // �[��1.0f�i�ő�l�j�ŃN���A
    depth_clear_value.Format = DXGI_FORMAT_D32_FLOAT;       // 32bit float�l�Ƃ��ăN���A

    auto result = m_Device->CreateCommittedResource(
        &depth_heap_prop,
        D3D12_HEAP_FLAG_NONE,
        &depth_res_desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,                   // �[�x�l�������݂ɗ��p
        nullptr,
        IID_PPV_ARGS(&m_DepthBuffer)
    );

    if (result != S_OK) {
        return false;
    }

    // �[�x�̂��߂̃f�B�X�N���v�^�q�[�v�쐬
    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{};
    dsv_heap_desc.NumDescriptors = 1;                       // �[�x�r���[�͂P��
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;    // �f�v�X�X�e���V���r���[�Ƃ��Ďg��
    result = m_Device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&m_DSV_Heap));
    if (result != S_OK) {
        return false;
    }

    // �[�x�r���[�쐬
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
    dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;                    // �[�x�l��32�r�b�g�g�p
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;     // 2D�e�N�X�`��
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

    // SRGB�����_�[�^�[�Q�b�g�r���[�ݒ�
    D3D12_RENDER_TARGET_VIEW_DESC rtvdesc{}; 
    //rtvdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

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

    // Z�o�b�t�@�ݒ�
    gpipeline.DepthStencilState.DepthEnable = true;
    gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;    // ���ׂď�������
    gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;         // �������ق����̗p
    gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;                                // �[�x�l�� 32bit float

    // ���̓��C�A�E�g�̐ݒ�
    gpipeline.InputLayout.pInputElementDescs = m_Model.GetVertexBuffer()->GetVertexLayout();
    gpipeline.InputLayout.NumElements = m_Model.GetVertexBuffer()->VertexLayoutLength();

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
    rootsig_desc.NumStaticSamplers = 2;

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

