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

class IVertexBuffer
{
public:

    virtual ~IVertexBuffer() {}
    
    virtual bool CreateVertexBuffer(const uint8_t* ptr, size_t size, size_t stride_size) = 0;
    virtual D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const = 0;
    virtual const D3D12_INPUT_ELEMENT_DESC* GetVertexLayout() const = 0;
    virtual int VertexLayoutLength() const = 0;

    virtual uint32_t VertexNum() const = 0;
};

class VertexBufferBase : public IVertexBuffer
{
public:

    VertexBufferBase();
    virtual ~VertexBufferBase();
    VertexBufferBase(VertexBufferBase&) = delete;
    VertexBufferBase& operator=(VertexBufferBase&) = delete;
    
    virtual bool CreateVertexBuffer(const uint8_t* ptr, size_t size, size_t stride_size);
    virtual D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
    virtual const D3D12_INPUT_ELEMENT_DESC* GetVertexLayout() const = 0;
    virtual int VertexLayoutLength() const = 0;

    virtual uint32_t VertexNum() const = 0;

private:

    ID3D12Resource*          m_VertBuff;
    D3D12_VERTEX_BUFFER_VIEW m_VbView;
};

class PMDData;
class VertexBufferPMD : public VertexBufferBase
{
public:

    VertexBufferPMD();
    virtual ~VertexBufferPMD();
    VertexBufferPMD(VertexBufferPMD&) = delete;
    VertexBufferPMD& operator=(VertexBufferPMD&) = delete;

    bool CreateVertexBuffer(const PMDData& pmd);
    virtual const D3D12_INPUT_ELEMENT_DESC* GetVertexLayout() const;
    virtual int VertexLayoutLength() const;
    
    virtual uint32_t VertexNum() const;

private:

    uint32_t m_VertexNum;
};

using VertexBufferPtr = std::shared_ptr<VertexBufferBase>;