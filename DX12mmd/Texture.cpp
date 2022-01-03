#include "DirectXTex.h"

#include "Texture.hpp"
#include "AppManager.hpp"
#include "Resource.hpp"
#include "Utility.hpp"


TextureGroup::TextureGroup()
    :
    m_ShaderResource(nullptr)
{}

TextureGroup::TextureGroup( ResourceManager* shader_resource )
    :
    m_ShaderResource( shader_resource )
{}

void TextureGroup::SetShaderResource(ResourceManager* shader_resource)
{
    m_ShaderResource = shader_resource;
}

bool TextureGroup::CreateTextures(
    const std::filesystem::path& filepath,
    TexturePtr* handle
)
{
    auto texture_cache = m_Textures.find(filepath.wstring());
    if (texture_cache != m_Textures.end()) {
        *handle = texture_cache->second;
        return true;
    }
    TexMetadata metadata{};
    ScratchImage scratch_img{};

    auto result = LoadFromWICFile(filepath.c_str(), WIC_FLAGS_NONE, &metadata, scratch_img);
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

    auto texture = std::make_shared<Texture>();
    m_Textures[filepath.wstring()] = texture;
    texture->Create(image_fmt, this);

    *handle = texture;

    return true;
}

bool TextureGroup::CreatePlaneTexture(
    const std::wstring& name,
    TexturePtr* handle,
    uint8_t color_r, uint8_t color_g, uint8_t color_b
)
{
    auto texture_cache = m_Textures.find(name);
    if (texture_cache != m_Textures.end()) {
        *handle = texture_cache->second;
        return true;
    }

    constexpr uint32_t image_width = 4;
    constexpr uint32_t image_height = 4;
    std::vector<uint8_t> plane_image(AlignmentedSize(image_width * 4, 256) * image_height);
    for (std::size_t i = 0; i < plane_image.size() / 4; ++i) {
        uint8_t* pixel = &plane_image[i * 4];
        pixel[0] = color_r;
        pixel[1] = color_g;
        pixel[2] = color_b;
        pixel[3] = 0xFF;
    }

    ImageFmt image_fmt = {
        image_height,
        image_width,
        AlignmentedSize(image_width * 4, 256),
        image_height * AlignmentedSize(image_width * 4, 256),
        1,
        1,
        1,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        plane_image.data(),
        static_cast<D3D12_RESOURCE_DIMENSION>(TEX_DIMENSION_TEXTURE2D)
    };

    auto texture = std::make_shared<Texture>();
    m_Textures[name] = texture;
    texture->Create(image_fmt, this);

    *handle = texture;

    return true;
}

bool TextureGroup::CreateGradationTexture(
    const std::wstring& name,
    TexturePtr* handle
)
{
    auto texture_cache = m_Textures.find(name);
    if (texture_cache != m_Textures.end()) {
        *handle = texture_cache->second;
        return true;
    }

    constexpr uint32_t image_width = 4;
    constexpr uint32_t image_height = 256;
    uint8_t color = 0xFF;
    std::vector<uint8_t> plane_image(AlignmentedSize(image_width * 4, 256) * image_height);
    for (std::size_t i = 0; i < 256; ++i) {
        uint8_t* pixel = &plane_image[i * 256];
        pixel[0] = color;
        pixel[1] = color;
        pixel[2] = color;
        pixel[3] = color;
        --color;
    }

    ImageFmt image_fmt = {
        image_height,
        image_width,
        AlignmentedSize(image_width * 4, 256),
        image_height * AlignmentedSize(image_width * 4, 256),
        1,
        1,
        1,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        plane_image.data(),
        static_cast<D3D12_RESOURCE_DIMENSION>(TEX_DIMENSION_TEXTURE2D)
    };

    auto texture = std::make_shared<Texture>();
    m_Textures[name] = texture;
    texture->Create(image_fmt, this);

    *handle = texture;

    return true;
}

bool TextureGroup::CreateShaderResourceView(
    const TexturePtr& handle,
    ResourceManager* resource_manager,
    const ResourceDescHandle& res_desc_handle
)
{
    if (!handle || resource_manager == nullptr || !res_desc_handle.Handle) {
        return false;
    }

   handle->CreateShaderResourceView(resource_manager, res_desc_handle);

    return true;
}

Texture::Texture()
    :
    m_ImageInfo(),
    m_TexBuff(nullptr),
    m_UploadBuff( nullptr ),
    m_Parent( nullptr )
{}

bool Texture::Create(const ImageFmt& image, TextureGroup* group)
{
    m_ImageInfo = image;
    m_UploadBuff = CreateTemporaryUploadResource(image);
    m_TexBuff = CreateTextureBuffer(image);
    m_Parent = group;

    if (m_UploadBuff == nullptr || m_TexBuff == nullptr) {
        return false;
    }

    uint8_t* mapforimg = nullptr;
    auto result = m_UploadBuff->Map(0, nullptr, reinterpret_cast<void**>(&mapforimg));
    if (result != S_OK) {
        return false;
    }

    auto srcaddr = image.Pixels;
    auto rowpitch = image.RowPitchByte;
    for (int y = 0; y < image.Height; ++y) {
        std::copy_n(srcaddr, rowpitch, mapforimg);

        srcaddr += rowpitch;
        mapforimg += AlignmentedSize(rowpitch, 256);
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
    src.PlacedFootprint.Footprint.RowPitch = AlignmentedSize( image.RowPitchByte, 256 );
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
    upload_resdesc.Width = image.Height * AlignmentedSize(image.RowPitchByte, 256);
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

void Texture::CreateShaderResourceView(ResourceManager* resource_manager, const ResourceDescHandle& res_desc_handle)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};

    srv_desc.Format = m_ImageInfo.Format;       // RGBA (0.0f - 1.0f に正規化)
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;                   // ミップマップは使用しないので1

    GraphicEngine::Instance().Device()->CreateShaderResourceView(
        m_TexBuff,
        &srv_desc,
        resource_manager->DescriptorHeapCPU(res_desc_handle)
    );
}