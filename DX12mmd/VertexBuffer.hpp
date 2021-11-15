#pragma once

#include <memory>
#include <d3d12.h>
#include <DirectXMath.h>

using namespace DirectX;
struct Vertex
{
    XMFLOAT3 pos;   
    XMFLOAT2 uv;
};

class VertexBuffer
{
public:

    VertexBuffer();
    VertexBuffer(VertexBuffer&) = delete;
    VertexBuffer& operator=(VertexBuffer&) = delete;
    
    bool CreateVertexBuffer(Vertex* vertices, size_t count);
    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;

private:

    ID3D12Resource*          m_VertBuff;
    D3D12_VERTEX_BUFFER_VIEW m_VbView;
};
using VertexBufferPtr = std::shared_ptr<VertexBuffer>;