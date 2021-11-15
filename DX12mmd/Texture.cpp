#include "DirectXTex.h"

#include "Texture.hpp"
#include "AppManager.hpp"
#include "Utility.hpp"


TextureGroup::TextureGroup()
    :
    m_TexDescHeap(nullptr),
    m_Textures(),
    m_RootParam(),
    m_DescRange(),
    m_Sampler()
{}

bool TextureGroup::CreateTextures(const wchar_t* filename)
{
    TexMetadata metadata{};
    ScratchImage scratch_img{};

    auto result = LoadFromWICFile(filename, WIC_FLAGS_NONE, &metadata, scratch_img);
    if (result != S_OK) {
        return false;
    }

    auto img = scratch_img.GetImage(0, 0, 0);
    ImageFmt image_fmt = {
        img->height,
        img->width,
        img->rowPitch,
        img->slicePitch,
        metadata.depth,
        metadata.arraySize,
        metadata.mipLevels,
        metadata.format,
        img->pixels,
        static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension)
    };

    CreateRootParameter();
    CreateSampler();
#if 0
    ImageFmt image = {
        256,
        256,
        AlignmentedSize( 256, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT ) * 4,
        256 * 256 * 4,
        1,
        1,
        1,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        nullptr,
        D3D12_RESOURCE_DIMENSION_TEXTURE2D
    };

    m_TextureData = std::vector<TexRGBA>(image.Width * image.Height);
    for (auto& rgba : m_TextureData) {
        rgba.R = rand() % 256;
        rgba.G = rand() % 256;
        rgba.B = rand() % 256;
        rgba.A = 255;
    }
    image.Pixels = reinterpret_cast<uint8_t*>(m_TextureData.data());
#endif




    if (!CreateDescriptorHeap()) {
        return false;
    }

    m_Textures.push_back(std::make_shared<Texture>());
    return m_Textures[0]->Create(image_fmt, this);
}

ID3D12DescriptorHeap* TextureGroup::TextureDescriptorHeap()
{
    return m_TexDescHeap;
}

D3D12_ROOT_PARAMETER* TextureGroup::RootParameter()
{
    return &m_RootParam;
}

D3D12_STATIC_SAMPLER_DESC* TextureGroup::SamplerDescriptor()
{
    return &m_Sampler;
}

bool TextureGroup::CreateDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};

    // シェーダーから見えるように
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    // マスクは0
    heap_desc.NodeMask = 0;
    // ビューは今のところ1つ
    heap_desc.NumDescriptors = 1;
    // シェーダーリソースビュー用
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    auto result = GraphicEngine::Instance().Device()->CreateDescriptorHeap(
        &heap_desc,
        IID_PPV_ARGS(&m_TexDescHeap)
    );

    return result == S_OK;
}

void TextureGroup::CreateRootParameter()
{
    m_DescRange.NumDescriptors = 1;         // テクスチャ数
    m_DescRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    m_DescRange.BaseShaderRegister = 0;     // 0番スロットから
    m_DescRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    // ピクセルシェーダから見える
    m_RootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    m_RootParam.DescriptorTable.pDescriptorRanges = &m_DescRange;
    m_RootParam.DescriptorTable.NumDescriptorRanges = 1;
}

void TextureGroup::CreateSampler()
{
    // 繰り返し
    m_Sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    m_Sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    m_Sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    m_Sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;    // ボーダーは黒
    m_Sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;                     // 線形補間は黒
    m_Sampler.MaxLOD = D3D12_FLOAT32_MAX;       // ミップマップ最大値
    m_Sampler.MinLOD = 0.0f;                    // ミップマップ最小値
    m_Sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;             // ピクセルシェーダから見える
    m_Sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;                 // リサンプリングしない
}


Texture::Texture()
    :
    m_ImageInfo(),
    m_TexBuff(nullptr),
    m_UploadBuff( nullptr ),
    m_Resource( nullptr )
{}

bool Texture::Create(const ImageFmt& image, TextureGroup* group)
{
    m_ImageInfo = image;
    m_UploadBuff = CreateTemporaryUploadResource(image);
    m_TexBuff = CreateTextureBuffer(image);
    m_Resource = group;

    if (m_UploadBuff == nullptr || m_TexBuff == nullptr) {
        return false;
    }

    uint8_t* mapforimg = nullptr;
    auto result = m_UploadBuff->Map(0, nullptr, reinterpret_cast<void**>(&mapforimg));
    if (result != S_OK) {
        return false;
    }

    uint64_t size = 0;
    auto srcaddr = image.Pixels;
    auto rowpitch = image.RowPitchByte;
    for (int y = 0; y < image.Height; ++y) {
        std::copy_n(srcaddr, rowpitch, mapforimg);

        srcaddr += rowpitch;
        mapforimg += rowpitch;
        size += rowpitch;
    }
    m_UploadBuff->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION src{};
    // コピー元（アップロード側）設定
    src.pResource = m_UploadBuff;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = 0;
    src.PlacedFootprint.Footprint.Width = image.Width;
    src.PlacedFootprint.Footprint.Height = image.Height;
    src.PlacedFootprint.Footprint.Depth = image.Depth;
    src.PlacedFootprint.Footprint.RowPitch = image.RowPitchByte;
    src.PlacedFootprint.Footprint.Format = image.Format;

    D3D12_TEXTURE_COPY_LOCATION dst{};
    //コピー先設定
    dst.pResource = m_TexBuff;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    auto cmdlist = GraphicEngine::Instance().CmdList();
    cmdlist->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    D3D12_RESOURCE_BARRIER barrier_desc{};
    barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_desc.Transition.pResource = m_TexBuff;
    barrier_desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier_desc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    cmdlist->ResourceBarrier(1, &barrier_desc);
    cmdlist->Close();

    GraphicEngine::Instance().ExecCmdQueue();
    CreateShaderResourceView();

    return true;
}

ID3D12Resource* Texture::CreateTemporaryUploadResource(const ImageFmt& image)
{
    // 中間バッファー作成
    D3D12_HEAP_PROPERTIES upload_heap_prop{};

    // マップ可能にするため、UPLOADを指定
    upload_heap_prop.Type = D3D12_HEAP_TYPE_UPLOAD;
    // アップロード用に使用する前提なので、UNKNOWNでよい
    upload_heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    upload_heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    // 単一アダプターのため0
    upload_heap_prop.CreationNodeMask = 0;
    upload_heap_prop.VisibleNodeMask = 0;

    // リソース設定
    D3D12_RESOURCE_DESC upload_resdesc{};
    upload_resdesc.Format = DXGI_FORMAT_UNKNOWN;                    // 単なるデータの塊なのでUNKNOWN
    upload_resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;     // 単なるバッファーとして指定
    upload_resdesc.Width = image.Size;
    upload_resdesc.Height = 1;
    upload_resdesc.DepthOrArraySize = image.ArraySize;
    upload_resdesc.MipLevels = image.MipLevels;

    upload_resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    upload_resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // 通常テクスチャなのでアンチエイリアスしない
    upload_resdesc.SampleDesc.Count = 1;
    upload_resdesc.SampleDesc.Quality = 0;

    ID3D12Resource* uploadbuff = nullptr;
    ID3D12Device* device = GraphicEngine::Instance().Device();
    auto result = device->CreateCommittedResource(
        &upload_heap_prop,
        D3D12_HEAP_FLAG_NONE,
        &upload_resdesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadbuff)
    );
    if (result != S_OK) {
        return nullptr;
    }

    return uploadbuff;
}

ID3D12Resource* Texture::CreateTextureBuffer(const ImageFmt& image)
{
    // テクスチャ用バッファー作成
    D3D12_HEAP_PROPERTIES tex_heap_prop{};

    // マップ可能にするため、UPLOADを指定
    tex_heap_prop.Type = D3D12_HEAP_TYPE_DEFAULT;
    // アップロード用に使用する前提なので、UNKNOWNでよい
    tex_heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    tex_heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    // 単一アダプターのため0
    tex_heap_prop.CreationNodeMask = 0;
    tex_heap_prop.VisibleNodeMask = 0;

    // リソース設定
    D3D12_RESOURCE_DESC texture_resdesc{};
    texture_resdesc.Format = image.Format;
    texture_resdesc.Width = image.Width;
    texture_resdesc.Height = image.Height;
    texture_resdesc.DepthOrArraySize = image.ArraySize;  // 2Dで配列でもないので1
    texture_resdesc.MipLevels = image.MipLevels;         // ミップマップしないのでミップ数は1   
    texture_resdesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(image.Dimension);

    texture_resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // 通常テクスチャなのでアンチエイリアスしない
    texture_resdesc.SampleDesc.Count = 1;
    texture_resdesc.SampleDesc.Quality = 0;

    ID3D12Resource* texbuff = nullptr;
    ID3D12Device* device = GraphicEngine::Instance().Device();
    auto result = device->CreateCommittedResource(
        &tex_heap_prop,
        D3D12_HEAP_FLAG_NONE,
        &texture_resdesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texbuff)
    );
    if (result != S_OK) {
        return nullptr;
    }

    return texbuff;
}



void Texture::CreateShaderResourceView()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};

    srv_desc.Format = m_ImageInfo.Format;       // RGBA (0.0f - 1.0f に正規化)
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;                   // ミップマップは使用しないので1

    GraphicEngine::Instance().Device()->CreateShaderResourceView(
        m_TexBuff,
        &srv_desc,
        m_Resource->TextureDescriptorHeap()->GetCPUDescriptorHandleForHeapStart()
    );
}