#include "AppManager.hpp"

#include <vector>

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
    m_Fence()
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

void GraphicEngine::FlipWindow()
{
    auto result = m_CmdAllocator->Reset();

    // �o�b�N�o�b�t�@�[�̃����_�[�^�[�Q�b�g�r���[���A���ꂩ�痘�p���郌���_�[�^�[�Q�b�g�r���[�ɐݒ�
    auto bbidx = m_Swapchain->GetCurrentBackBufferIndex();
    auto rtvH = m_RtvHeaps->GetCPUDescriptorHandleForHeapStart();
    rtvH.ptr += bbidx * m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // ���\�[�X�o���A�ݒ�
    SetRenderTargetResourceBarrier(bbidx, true);
    m_CmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

    // �����_�[�^�[�Q�b�g�N���A
    float clear_color[] = { 1.0f, 1.0f, 0.0f, 1.0f };
    m_CmdList->ClearRenderTargetView(rtvH, clear_color, 0, nullptr);

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

    //���\�[�X�o���A������
    SetRenderTargetResourceBarrier(bbidx, false);

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

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RtvHeaps->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < backbuffers.size(); ++i) {
        // �����_�[�^�[�Q�b�g�r���[�𐶐�����
        m_Device->CreateRenderTargetView(backbuffers[i], nullptr, handle);
        // �|�C���^�[�����炷
        handle.ptr += m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    return true;
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
    barrier_desc.Transition.Subresource = 0;

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


