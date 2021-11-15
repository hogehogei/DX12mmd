#pragma once

#include <memory>
#include <d3d12.h>
#include <DirectXMath.h>

class IndexBuffer
{
public:

    IndexBuffer();
    IndexBuffer(IndexBuffer&) = delete;
    IndexBuffer& operator=(IndexBuffer&) = delete;

    size_t Count() const;
    bool CreateIndexBuffer(uint16_t* indices, size_t count);
    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;

private:

    ID3D12Resource* m_IndicesBuff;
    size_t m_IdxCount;
    D3D12_INDEX_BUFFER_VIEW m_IdxView;
};
using IndexBufferPtr = std::shared_ptr<IndexBuffer>;