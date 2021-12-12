#include "DirectXTex.h"

#include "Texture.hpp"
#include "AppManager.hpp"
#include "Resource.hpp"
#include "Utility.hpp"


TextureGroup::TextureGroup()
    :
    m_ShaderResource( nullptr )
{}

bool TextureGroup::CreateTextures(
    ResourceManager* shader_resource, 
    const ResourceDescHandle& shader_resource_desc_handle,
    const wchar_t* filename, 
    TextureHandle* handle
)
{
    if (!shader_resource_desc_handle) {
        return false;
    }

    TexMetadata metadata{};
    ScratchImage scratch_img{};

    m_ShaderResource = shader_resource;

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

    auto texture = std::make_shared<Texture>();
    m_Textures.push_back(texture);
    texture->Create(image_fmt, this, shader_resource_desc_handle);

    *handle = m_Textures.size() - 1;

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureGroup::TextureDescriptorHeapCPU(TextureHandle handle)
{
    assert(handle != Texture::k_InvalidHandle);
    m_Textures[handle]->GetResourceDescHandle();
    return m_ShaderResource->DescriptorHeapCPU(m_Textures[handle]->GetResourceDescHandle());
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureGroup::TextureDescriptorHeapGPU(TextureHandle handle)
{
    assert(handle != Texture::k_InvalidHandle);
    return m_ShaderResource->DescriptorHeapGPU(m_Textures[handle]->GetResourceDescHandle());
}


TextureHandle TextureGroup::GetTextureHandle(Texture* texptr)
{  
    for (size_t i = 0; i < m_Textures.size(); ++i) {
        if (m_Textures[i].get() == texptr) {
            return i;
        }
    }

    return Texture::k_InvalidHandle;
}

Texture::Texture()
    :
    m_ImageInfo(),
    m_TexBuff(nullptr),
    m_UploadBuff( nullptr ),
    m_Parent( nullptr ),
    m_ResourceDescHandle()
{}

bool Texture::Create(const ImageFmt& image, TextureGroup* group, const ResourceDescHandle& res_desc_handle)
{
    m_ImageInfo = image;
    m_UploadBuff = CreateTemporaryUploadResource(image);
    m_TexBuff = CreateTextureBuffer(image);
    m_Parent = group;
    m_ResourceDescHandle = res_desc_handle;

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

    GraphicEngine::Instance().ExecCmdQueue();
    CreateShaderResourceView();

    return true;
}

ResourceDescHandle Texture::GetResourceDescHandle() const
{
    return m_ResourceDescHandle;
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

    auto handle = m_Parent->GetTextureHandle(this);
    GraphicEngine::Instance().Device()->CreateShaderResourceView(
        m_TexBuff,
        &srv_desc,
        m_Parent->TextureDescriptorHeapCPU(handle)
    );
}