#include "d3dx12.h"

#include "VertexBuffer.hpp"
#include "AppManager.hpp"
#include "PMD.hpp"

namespace
{
    // 座標情報
    D3D12_INPUT_ELEMENT_DESC s_PMDVertexLayout[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },
        // 法線
        {
            "NORMAL",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },
        // uv
        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },
        // ボーン番号
        {
            "BONE_NO",
            0,
            DXGI_FORMAT_R16G16_UINT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },
        // 重み
        {
            "WEIGHT",
            0,
            DXGI_FORMAT_R8_UINT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },
        // エッジフラグ
#if 0
        {
            "EDGE_FLG",
            0,
            DXGI_FORMAT_R8_UINT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },
#endif
    };
}

VertexBufferBase::VertexBufferBase()
    :
    m_VertBuff(nullptr),
    m_VbView()
{}

VertexBufferBase::~VertexBufferBase()
{}

bool VertexBufferBase::CreateVertexBuffer(const uint8_t* ptr, size_t size, size_t stride_size)
{
    ID3D12Device* device = GraphicEngine::Instance().Device();

    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto res_desc = CD3DX12_RESOURCE_DESC::Buffer(size);

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

    uint8_t* vertmap = nullptr;
   
    result = m_VertBuff->Map(0, nullptr, reinterpret_cast<void**>(&vertmap));
    if (result != S_OK) {
        return false;
    }

    std::copy_n(ptr, size, vertmap);
    m_VertBuff->Unmap(0, nullptr);

    m_VbView = D3D12_VERTEX_BUFFER_VIEW();
    m_VbView.BufferLocation = m_VertBuff->GetGPUVirtualAddress();
    m_VbView.SizeInBytes = size;
    m_VbView.StrideInBytes = stride_size;

    return true;
}

D3D12_VERTEX_BUFFER_VIEW VertexBufferBase::GetVertexBufferView() const
{
    return m_VbView;
}


VertexBufferPMD::VertexBufferPMD()
    :
    m_VertexNum(0)
{}

VertexBufferPMD::~VertexBufferPMD()
{}

bool VertexBufferPMD::CreateVertexBuffer(const PMDData& pmd)
{
    m_VertexNum = pmd.VertexNum();
    return VertexBufferBase::CreateVertexBuffer(
        pmd.GetVertexData(),
        pmd.VertexBuffSize(),
        pmd.VertexStrideByte()
    );
}

const D3D12_INPUT_ELEMENT_DESC* VertexBufferPMD::GetVertexLayout() const
{
    return &s_PMDVertexLayout[0];
}

int VertexBufferPMD::VertexLayoutLength() const
{
    return sizeof(s_PMDVertexLayout) / sizeof(s_PMDVertexLayout[0]);
}

uint32_t VertexBufferPMD::VertexNum() const
{
    return m_VertexNum;
}