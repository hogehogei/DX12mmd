#include "d3dx12.h"

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

    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto res_desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(Vertex) * count);

    auto result = device->CreateCommittedResource(
        &heap_prop,
        D3D12_HEAP_FLAG_NONE,
        &res_desc,
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
