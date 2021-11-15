
#include "IndexBuffer.hpp"
#include "AppManager.hpp"

IndexBuffer::IndexBuffer()
    :
    m_IndicesBuff(nullptr),
    m_IdxCount(0),
    m_IdxView()
{}

size_t IndexBuffer::Count() const
{
    return m_IdxCount;
}

bool IndexBuffer::CreateIndexBuffer(uint16_t* indices, size_t count)
{
    if (count == 0) {
        return false;
    }

    ID3D12Device* device = GraphicEngine::Instance().Device();
    D3D12_HEAP_PROPERTIES heapprop{};

    heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resdesc{};

    resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    // 頂点情報が入るだけのサイズを指定
    resdesc.Width = static_cast<UINT64>(sizeof(uint16_t)) * count;
    // 幅で表現するので1とする
    resdesc.Height = 1;
    resdesc.DepthOrArraySize = 1;
    resdesc.MipLevels = 1;
    resdesc.Format = DXGI_FORMAT_UNKNOWN;
    resdesc.SampleDesc.Count = 1;
    resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto result = device->CreateCommittedResource(
        &heapprop,
        D3D12_HEAP_FLAG_NONE,
        &resdesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_IndicesBuff)
    );
    if (result != S_OK) {
        return false;
    }

    uint16_t* idxmap = nullptr;

    result = m_IndicesBuff->Map(0, nullptr, reinterpret_cast<void**>(&idxmap));
    if (result != S_OK) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        idxmap[i] = indices[i];
    }
    m_IndicesBuff->Unmap(0, nullptr);

    m_IdxView = D3D12_INDEX_BUFFER_VIEW();
    m_IdxView.BufferLocation = m_IndicesBuff->GetGPUVirtualAddress();
    m_IdxView.Format = DXGI_FORMAT_R16_UINT;
    m_IdxView.SizeInBytes = sizeof(indices[0]) * count;
    m_IdxCount = count;

    return true;
}

D3D12_INDEX_BUFFER_VIEW IndexBuffer::GetIndexBufferView() const
{
    return m_IdxView;
}
