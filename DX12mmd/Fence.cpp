
#include "Fence.hpp"

Fence::Fence()
    : 
    m_FenceValue( 0 ),
    m_Event( nullptr ),
    m_Device( nullptr ),
    m_CmdQueue( nullptr ),
    m_Fence( nullptr )
{}

bool Fence::Initialize(ID3D12Device* device, ID3D12CommandQueue* cmdqueue)
{
    m_FenceValue = 0;
    m_Event = CreateEvent(nullptr, false, false, nullptr);
    m_Device = device;
    m_CmdQueue = cmdqueue;
   
    auto result = m_Device->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));

    return result == S_OK;
}

void Fence::WaitCmdComplete()
{
    ++m_FenceValue;
    m_CmdQueue->Signal(m_Fence, m_FenceValue);
    // fence ‚ÉÝ’è‚·‚é FenceValue ‚Í 1ˆÈã‚Å‚È‚¢‚Æ‚¢‚¯‚È‚¢
    // 0‚¾‚Æ“¯Šú‚¹‚¸‚·‚®‚É–ß‚Á‚Ä‚«‚Ä‚µ‚Ü‚¤
    m_Fence->SetEventOnCompletion(m_FenceValue, m_Event);
    WaitForSingleObject(m_Event, INFINITE);
}
