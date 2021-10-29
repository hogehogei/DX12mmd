#pragma once

#include <d3d12.h>

class Fence
{
public:

    Fence();

    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* cmdqueue);
    void WaitCmdComplete();

private:

    UINT64 m_FenceValue;
    HANDLE m_Event;

    ID3D12Device* m_Device;
    ID3D12CommandQueue* m_CmdQueue;
    ID3D12Fence* m_Fence;
};