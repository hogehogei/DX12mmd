
#include "VertexBuffer.hpp"
#include "AppManager.hpp"

VertexBuffer::VertexBuffer()
    :
    m_VertBuff(nullptr),
    m_VbView()
{}

bool VertexBuffer::CreateVertexBuffer(Vertex* vertices, size_t count)
{
    ID3D12Device* device = GraphicEngine::Instance().Device();
    D3D12_HEAP_PROPERTIES heapprop{};

    heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resdesc{};

    resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    // 頂点情報が入るだけのサイズを指定
    resdesc.Width = static_cast<UINT64>(sizeof(Vertex)) * count;
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
        IID_PPV_ARGS(&m_VertBuff)
    );
    if (result != S_OK) {
        return false;
    }

    Vertex* vertmap = nullptr;
   
    result = m_VertBuff->Map(0, nullptr, reinterpret_cast<void**>(&vertmap));
    if (result != S_OK) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        vertmap[i] = vertices[i];
    }
    m_VertBuff->Unmap(0, nullptr);

    m_VbView = D3D12_VERTEX_BUFFER_VIEW();
    m_VbView.BufferLocation = m_VertBuff->GetGPUVirtualAddress();
    m_VbView.SizeInBytes = sizeof(vertices[0]) * count;
    m_VbView.StrideInBytes = sizeof(vertices[0]);

    return true;
}

D3D12_VERTEX_BUFFER_VIEW VertexBuffer::GetVertexBufferView() const
{
    return m_VbView;
}
