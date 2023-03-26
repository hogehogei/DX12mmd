#include <sstream>
#include <array>
#include <algorithm>

#include "PMDActor.hpp"
#include "FilePath.hpp"

#pragma comment(lib, "winmm.lib")

namespace
{
    void WriteMaterial(void* srcdata, uint8_t* dst)
    {
        PMDData* data = reinterpret_cast<PMDData*>(srcdata);
        const auto& material_data = data->GetMaterialData();
        uint32_t material_buff_size = (PMDData::k_ShaderMaterialSize + 0xFF) & ~0xFF;

        for (const auto& itr : material_data) {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(&itr.MaterialForShader);
            std::copy_n(src, PMDData::k_ShaderMaterialSize, dst);
            dst += material_buff_size;
        }
    }
}

PMDActor::PMDActor()
    :
    m_PMDModelPath(),
    m_VMDMotionPath(),
    m_ModelName(),
    m_ResourceManager(nullptr),
    m_PMDData(),
    m_VertBuff(std::make_shared<VertexBufferPMD>()),
    m_IdxBuff(),
    m_MaterialBuff(),
    m_BoneMetricesForMotion()
{}

bool PMDActor::Create(
    ResourceManager* resource_manager, 
    const std::string& model_name, 
    const std::filesystem::path& pmd_filepath,
    const std::filesystem::path& vmd_filepath
)
{
    if (!m_PMDData.Open(pmd_filepath)) {
        return false;
    }
    if (!m_VMDData.Open(vmd_filepath)) {
        return false;
    }

    m_ResourceManager = resource_manager;
    m_TextureManager.SetShaderResource(m_ResourceManager);
    m_PMDModelPath = pmd_filepath;
    m_VMDMotionPath = vmd_filepath;
    m_ModelName = model_name;

    auto vertbuff = std::make_shared<VertexBufferPMD>();
    if (!vertbuff->CreateVertexBuffer(m_PMDData)) {
        return false;
    }
    m_VertBuff = vertbuff;

    auto idxbuff = std::make_shared<IndexBuffer>();
    if (!idxbuff->CreateIndexBuffer(m_PMDData)) {
        return false;
    }
    m_IdxBuff = idxbuff;

    ResourceOrder material_resource =
    {
        model_name,
        {
            {ResourceOrder::k_ConstantResource, 1, 1},
            {ResourceOrder::k_ShaderResource, 4, 0 }
        },
        m_PMDData.MaterialNum()
    };
    resource_manager->AddResource(material_resource);

    m_MaterialBuff = std::make_shared<ConstantBuffer>();
    m_MaterialBuff->Create(resource_manager, PMDData::k_ShaderMaterialSize, m_PMDData.MaterialNum(), resource_manager->ResourceHandle(model_name));
    ConstantBuffer::WriterFunc func = WriteMaterial;
    m_MaterialBuff->Write((void*)&m_PMDData, func);

    // GPU転送用ボーン行列バッファ初期化
    m_BoneMetricesForMotion.resize(k_BoneMetricesNum);
    std::fill(m_BoneMetricesForMotion.begin(), m_BoneMetricesForMotion.end(), DirectX::XMMatrixIdentity());

    CreateTextures();

    return true;
}

VertexBufferPtr PMDActor::GetVertexBuffer()
{
    return m_VertBuff;
}

IndexBufferPtr PMDActor::GetIndexBuffer()
{
    return m_IdxBuff;
}

ConstantBufferPtr PMDActor::GetMaterialBuffer()
{
    return m_MaterialBuff;
}

const PMDData& PMDActor::GetPMDData() const
{
    return m_PMDData;
}

const VMDMotionTable& PMDActor::GetVMDMotionTable() const
{
    return m_VMDData;
}

void PMDActor::RecursiveMatrixMultiply(
    const BoneTree::BoneNode* node, const DirectX::XMMATRIX& mat
)
{
    if (!node) {
        return;
    }

    m_BoneMetricesForMotion[node->BoneIdx] *= mat;
    
    for (const auto child_node : node->GetChildlen())
    {
        RecursiveMatrixMultiply(child_node, m_BoneMetricesForMotion[node->BoneIdx]);
    }
}

std::vector<DirectX::XMMATRIX>& PMDActor::GetBoneMetricesForMotion()
{
    return m_BoneMetricesForMotion;
}

void PMDActor::PlayAnimation()
{
    m_AnimeStartTimeMs = timeGetTime();
}

void PMDActor::MotionUpdate()
{
    DWORD elapsed_time = timeGetTime() - m_AnimeStartTimeMs;
    DWORD frame_no = 30 * (elapsed_time / 1000.0f);

    if (frame_no > m_VMDData.MaxKeyFrameNo()) {
        PlayAnimation();
        elapsed_time = 0;
    }

    std::fill(m_BoneMetricesForMotion.begin(), m_BoneMetricesForMotion.end(), DirectX::XMMatrixIdentity());
    VMDMotionTable::NowMotionListPtr motions = m_VMDData.GetNowMotionList(frame_no);

    for (const auto& motion : *motions) {
        auto node = m_PMDData.GetBoneFromName(motion.Name);
        if (node) {
            auto idx = node.value().BoneIdx;
            auto& pos = node.value().BoneStartPos;

            DirectX::XMMATRIX rotation;
            DirectX::XMVECTOR offset;
            motion.Slerp(frame_no, &rotation, &offset);
            auto mat =
                DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z) *
                rotation *
                DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

            m_BoneMetricesForMotion[idx] = mat * DirectX::XMMatrixTranslationFromVector(offset);
        }
    }

    auto root_bone = m_PMDData.GetBoneFromName("センター");
    if (root_bone) {
        RecursiveMatrixMultiply(&(root_bone.value()), DirectX::XMMatrixIdentity());
    }

    IKSolve(frame_no);
}

void PMDActor::CreateTextures()
{
    const auto& materials = m_PMDData.GetMaterialData();
    auto shader_resource_handle = m_ResourceManager->ResourceHandle(m_ModelName);

    for (const auto& m : materials) {
        std::filesystem::path filepath = m.Additional.TexturePath;
        TexturePtr texture_handle;
        TexturePath texture_path;

        texture_path = GetTexturePathFromModelAndTexPath(m_PMDModelPath, filepath);
        
        shader_resource_handle.Offset += 1;
        if (!texture_path.TexPath.empty()) {
            m_TextureManager.CreateTextures(
                texture_path.TexPath,
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }
        else {
            m_TextureManager.CreatePlaneTexture(
                L"white",
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }

        shader_resource_handle.Offset += 1;
        if (!texture_path.SphereMapPath.empty()) {
            m_TextureManager.CreateTextures(
                texture_path.SphereMapPath,
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }
        else {
            m_TextureManager.CreatePlaneTexture(
                L"white",
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }

        shader_resource_handle.Offset += 1;
        if (!texture_path.AddSphereMapPath.empty()) {
            m_TextureManager.CreateTextures(
                texture_path.AddSphereMapPath,
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }
        else {
            m_TextureManager.CreatePlaneTexture(
                L"black",
                &texture_handle,
                0x00, 0x00, 0x00
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }

        shader_resource_handle.Offset += 1;
        ReadToonTexture(m, shader_resource_handle);

        shader_resource_handle.Offset += 1;
    }
}

void PMDActor::ReadToonTexture(const Material& material, const ResourceDescHandle& handle)
{
    std::ostringstream os;
    std::filesystem::path filepath;

    // 実際のtoon番号は 記録された番号+1される（uint8_t サイズでWrap）
    uint8_t toonidx = (material.Additional.ToonIdx + 1);
    filepath += L"toon/toon";
    os << std::setfill('0') << std::right << std::setw(2) << static_cast<int>(toonidx);

    // string を wstring に変換
    // ASCII しか使ってないと断言できるのでこのようにしている
    std::string number_str = os.str();
    std::vector<wchar_t> wstr;
    std::copy(number_str.begin(), number_str.end(), std::back_inserter(wstr));
    wstr.push_back(L'\0');

    filepath += wstr.data();
    filepath += L".bmp";

    std::error_code ec;
    bool result = std::filesystem::exists(filepath, ec);
    TexturePtr texture_handle;
    if (!ec && result) {
        m_TextureManager.CreateTextures(
            filepath,
            &texture_handle
        );
        m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, handle);
        m_Textures.push_back(texture_handle);
    }
    else {
        m_TextureManager.CreateGradationTexture(
            L"gradation",
            &texture_handle
        );
        m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, handle);
        m_Textures.push_back(texture_handle);
    }
}

// @brief Z軸を特定の方向に向ける行列を作成する
// @param up    上ベクトル
// @param right 右ベクトル
XMMATRIX PMDActor::CreateLookAtMatrixZ(
    const XMVECTOR& lookat,
    const XMFLOAT3& up,
    const XMFLOAT3& right
)
{
    // 向かせたい方向(Z軸)
    XMVECTOR vz = lookat;

    // 向かせたい方向を向かせたときの、仮のYベクトル
    XMVECTOR vy_tmp = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&up));

    // 外積1回目。仮のY軸を使って、向かせたい方向を向かせたときの本当のX, Yベクトルを計算したい
    // 1. Z軸と仮のY軸を外積して、まずはX軸を計算
    XMVECTOR vx = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vy_tmp, vz));
    // 外積2回目
    // 2. Z軸とX軸を外積して、本物のY軸を計算
    XMVECTOR vy = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vz, vx));

    // Lookatとupが同じ方向を向いていたらrightを基準にして作り直し
    if ((std::abs(DirectX::XMVector3Dot(vy, vz).m128_f32[0]) - 1.0f) < std::numeric_limits<float>::epsilon()) {
        // 本物のX軸を計算したい。まずは right=仮のX軸とする
        XMVECTOR vx_tmp = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&right));

        // 外積1回目 Z軸と仮のX軸を外積して、Y軸を計算
        vy = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vz, vx_tmp));
        // 外積2回目 Z軸とY軸を外積して、本物のX軸を計算
        vx = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vy, vz));
    }

    XMMATRIX ret = DirectX::XMMatrixIdentity();
    ret.r[0] = vx;
    ret.r[1] = vy;
    ret.r[2] = vz;

    return ret;
}

XMMATRIX PMDActor::CreateLookAtMatrix(
    const XMVECTOR& origin,
    const XMVECTOR& lookat,
    const XMFLOAT3& up,
    const XMFLOAT3& right
)
{
    // 1. Z軸を任意のベクトルに向ける行列を計算する
    // 2. 1.で作成した行列の逆行列を作成（Ztrans)
    // 3. Z軸を特定の方向に向ける行列を作成 (Zlookat)
    // 4. 1(Ztrans)と4(Zlookat)で作成した行列を乗算。すると、任意のベクトル(origin)を Ztrans -> Zlookat に回転させる行列ができる。
    //    Ztrans -> Zlookat に旋回するベクトルを作成しているイメージ
    return DirectX::XMMatrixTranspose(CreateLookAtMatrixZ(origin, up, right)) * CreateLookAtMatrixZ(lookat, up, right);
}

void PMDActor::IKSolve(uint32_t frame_no)
{
    const auto& iks = m_PMDData.GetPMDIKData();
    for (const auto& ik : iks) {
        // IK ON/OFFデータ確認
        auto ik_enable_list = m_VMDData.GetIKEnable(frame_no);
        if (ik_enable_list) {
            auto itr = ik_enable_list->IkEnableTable.find(m_PMDData.GetBoneName(ik.BoneIdx));
            if (itr != ik_enable_list->IkEnableTable.end()) {
                // もしOFFなら、そのボーンのIK処理しない
                if (!itr->second) {
                    continue;
                }
            }
        }

        auto children_nodes_count = ik.NodeIdxes.size();

        switch (children_nodes_count) {
        case 0:     // 間のボーン数が0(ありえない)
            assert(0);
            break;
        case 1:     // 間のボーン数が1の時はLookAt
            SolveLookAt(ik);
            break;
        case 2:     // 間のボーン数が2の時は余弦定理IK
            SolveCosineIK(ik);
            break;
        default:
            SolveCCDIK(ik);
            break;
        }
    }
}

void PMDActor::SolveLookAt(const PMDIK& ik)
{
    // この関数に来た時点でノードはひとつしかなく、チェーンに入っているノード番号は
    // IKのルートノードのものなので、このルートノードからターゲットに向かうベクトルを考えればよい
    auto root_node = m_PMDData.GetBoneFromIndex(ik.NodeIdxes[0]);
    auto target_node = m_PMDData.GetBoneFromIndex(ik.TargetIdx);    // 末端ボーン
    assert(root_node && target_node);

    // IKを動かす前のルート->末端のベクトルを取得
    auto rootpos1 = DirectX::XMLoadFloat3(&(root_node->BoneStartPos));
    auto targetpos1 = DirectX::XMLoadFloat3(&(target_node->BoneStartPos));

    // IKを動かした後のルート->末端（目標ボーン）のベクトルを取得（目標位置への移動）
    auto rootpos2 = DirectX::XMVector3TransformCoord(rootpos1, m_BoneMetricesForMotion[ik.NodeIdxes[0]]);   // 座標変換後のルートノードの座標取得
    auto targetpos2 = DirectX::XMVector3TransformCoord(targetpos1, m_BoneMetricesForMotion[ik.BoneIdx]);    // 座標変換後の目標ボーンの座標取得。
                                                                                                            // 座標変換行列はIKボーンの行列を用いることで、末端を目標へ移動。

    // IKで移動するベクトルを計算
    auto origin_vec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetpos1, rootpos1));
    auto target_vec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetpos2, rootpos2));

    // IKで移動する回転行列を作成
    m_BoneMetricesForMotion[ik.NodeIdxes[0]] =
        DirectX::XMMatrixTranslationFromVector(-rootpos2) *
        CreateLookAtMatrix(origin_vec, target_vec, DirectX::XMFLOAT3(0.f, 1.f, 0.f), DirectX::XMFLOAT3(1.f, 0.f, 0.f)) *
        DirectX::XMMatrixTranslationFromVector(rootpos2);
}

void PMDActor::SolveCosineIK(const PMDIK& ik)
{
    // IK構成点を保存
    std::vector<XMVECTOR> positions;
    // IKのそれぞれのボーン間の距離を保存
    std::array<float, 2> edge_lens;

    // 座標変換後のIKボーンの座標を取得
    // ターゲット（末端ボーンではなく、末端ボーンが近づく目標ボーンの座標を取得）
    auto target_node = m_PMDData.GetBoneFromIndex(ik.BoneIdx);
    assert(target_node);
    auto target_pos = DirectX::XMVector3Transform(
        DirectX::XMLoadFloat3(&(target_node->BoneStartPos)),
        m_BoneMetricesForMotion[ik.BoneIdx]
    );

    // IKチェーンは末端からファイルに記録されているので、逆にしておく
    // 末端ボーン
    auto end_node = m_PMDData.GetBoneFromIndex(ik.TargetIdx);
    assert(end_node);
    positions.emplace_back(DirectX::XMLoadFloat3(&(end_node->BoneStartPos)));
    // 中間及びルートボーン
    for (auto& chain_bone_idx : ik.NodeIdxes) {
        auto bone_node = m_PMDData.GetBoneFromIndex(chain_bone_idx);
        assert(bone_node);
        positions.emplace_back(DirectX::XMLoadFloat3(&(bone_node->BoneStartPos)));
    }
    // ルートから末端の順へ
    std::reverse(positions.begin(), positions.end());

    // ベクトルの元々の長さを測っておく
    edge_lens[0] = DirectX::XMVector3Length(DirectX::XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
    edge_lens[1] = DirectX::XMVector3Length(DirectX::XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

    // ルートボーン座標変換（逆順になっているので、使用するインデックスに注意）
    positions[0] = DirectX::XMVector3Transform(positions[0], m_BoneMetricesForMotion[ik.NodeIdxes[1]]);
    // 真ん中は自動計算されるので計算しない
    // positions[1];
    // 末端ボーン座標変換
    positions[2] = DirectX::XMVector3Transform(positions[2], m_BoneMetricesForMotion[ik.BoneIdx]);  // IKボーンの座標変換行列を指定（末端はIKに追従するので）

    // ルートから先端へのベクトルを作っておく
    auto linear_vec = DirectX::XMVectorSubtract(positions[2], positions[0]);

    double A = DirectX::XMVector3Length(linear_vec).m128_f32[0];
    double B = edge_lens[0];
    double C = edge_lens[1];
    double Apow2 = A * A;
    double Bpow2 = B * B;
    double Cpow2 = C * C;

    if( (A < std::numeric_limits<double>::epsilon()) ||
        (B < std::numeric_limits<double>::epsilon()) ||
        (C < std::numeric_limits<double>::epsilon()) ) {
        // どれか一辺が0に近しいのでデータがおかしい。何もしない。
        return;
    }

    linear_vec = DirectX::XMVector3Normalize(linear_vec);

    // ルートから真ん中への角度計算
    double theta1 = std::acos((Apow2 + Bpow2 - Cpow2) / (2 * A * B));
    // 真ん中からターゲットへの角度計算
    double theta2 = std::acos((Bpow2 + Cpow2 - Apow2) / (2 * B * C));

    // 軸を求める
    // もし真ん中が「ひざ」であった場合、強制的にX軸を回転軸とする
    XMVECTOR axis;
    const auto& knee_indexes = m_PMDData.GetBoneIndexes();
    if (std::find(knee_indexes.begin(), knee_indexes.end(), ik.NodeIdxes[0]) == knee_indexes.end()) {
        // 「ひざ」でない場合
        // ルート -> 末端 ベクトルと、ルート -> 目標(IK移動先) ベクトルのなす平面を考え、その外積ベクトルを軸とする
        auto vm = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(positions[2], positions[0]));
        auto vt = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target_pos, positions[0]));
        axis = DirectX::XMVector3Cross(vm, vt);
    }
    else {
        // 「ひざ」の場合
        auto right = DirectX::XMFLOAT3(1, 0, 0);
        axis = DirectX::XMLoadFloat3(&right);
    }

    // TODO: 角度制限を入れてないので、ひじとかがおかしな方向に曲がる可能性有り
    // 注意点：IKチェーンはルートに向かって数えられるので、1がルートに近い
    // ルート -> 中間点 を回す行列
    auto mat1 = DirectX::XMMatrixTranslationFromVector(-positions[0]);
    mat1 *= DirectX::XMMatrixRotationAxis(axis, theta1);
    mat1 *= DirectX::XMMatrixTranslationFromVector(positions[0]);
    // 中間点 -> 末端 を回す行列
    auto mat2 = DirectX::XMMatrixTranslationFromVector(-positions[1]);
    mat2 *= DirectX::XMMatrixRotationAxis(axis, theta2-DirectX::XM_PI);     // (180 - theta2)° 逆回転する
    mat2 *= DirectX::XMMatrixTranslationFromVector(positions[1]);

    // ルートを回す
    m_BoneMetricesForMotion[ik.NodeIdxes[1]] *= mat1;
    // 中間点を回す(先に中間点を回してから、ルートを回さないと、中間点の回転軸の中心がずれるので注意)
    m_BoneMetricesForMotion[ik.NodeIdxes[0]] = mat2 * m_BoneMetricesForMotion[ik.NodeIdxes[1]];
    // 末端を回す
    m_BoneMetricesForMotion[ik.TargetIdx] = m_BoneMetricesForMotion[ik.NodeIdxes[0]];
}

void PMDActor::SolveCCDIK(const PMDIK& ik)
{
    // ターゲット（末端ボーンではなく、末端ボーンが近づく目標ボーンの座標を取得）
    auto target_bone_node = m_PMDData.GetBoneFromIndex(ik.BoneIdx);
    assert(target_bone_node);
    auto target_origin_pos = DirectX::XMLoadFloat3(&(target_bone_node->BoneStartPos));
    // 親IKノードの座標変換が計算の邪魔なので、逆行列でいったん無効化
    // (PMDデータを見ると、LookAt, 余弦定理IKの場合は親IKノードが存在しないので、親IKノードの座標変換は考慮する必要がないが
    // CCDIKの場合は親IKノードがかならず存在するので、親の変換行列をいったん無効化しておく必要がある）
    auto parent_bone_idx = m_PMDData.GetBoneFromIndex(ik.BoneIdx)->IkParentBone;
    assert(parent_bone_idx < m_BoneMetricesForMotion.size());
    auto parent_mat = m_BoneMetricesForMotion[parent_bone_idx];
    DirectX::XMVECTOR det;
    auto inv_parent_mat = DirectX::XMMatrixInverse(&det, parent_mat);
    auto target_next_pos = DirectX::XMVector3Transform(
        target_origin_pos, m_BoneMetricesForMotion[ik.BoneIdx] * inv_parent_mat
    );

    // 各ボーンの現在座標を保存
    std::vector<XMVECTOR> bone_positions;
    // 末端ノード
    auto end_node = m_PMDData.GetBoneFromIndex(ik.TargetIdx);
    assert(end_node);
    auto end_pos = DirectX::XMLoadFloat3(&(end_node->BoneStartPos));
    // 中間ノード
    for (auto cidx : ik.NodeIdxes) {
        const auto& n = m_PMDData.GetBoneFromIndex(cidx);
        assert(n);
        bone_positions.emplace_back(
            DirectX::XMLoadFloat3(&(n->BoneStartPos))
        );
    }

    // 回転行列を記憶しておく
    std::vector<XMMATRIX> mats(bone_positions.size(), DirectX::XMMatrixIdentity());
    const auto ik_limit = ik.Limit * DirectX::XM_PI;    // 度数法を180で割った値を記録してる？ ラジアンとして使用するので、XM_PIをかける

    constexpr double epsilon = 0.0005;
    // IKに設定されている試行回数だけ繰り返す
    for (int c = 0; c < ik.Iterations; ++c) {
        // ターゲットとほぼ一致したら抜ける
        if (DirectX::XMVector3Length(DirectX::XMVectorSubtract(end_pos, target_next_pos)).m128_f32[0] <= epsilon) {
            break;
        }

        // それぞれのボーンをさかのぼりながら
        // 角度制限に引っかからないように曲げていく
        for (int bidx = 0; bidx < bone_positions.size(); ++bidx) {
            // いま曲げようとしているボーン位置
            const auto& pos = bone_positions[bidx];

            // 対象ノードから末端ノードまでと、
            // 対象ノードからターゲットまでのベクトル作成
            auto vec_to_end = DirectX::XMVectorSubtract(end_pos, pos);              // 末端へ
            auto vec_to_target = DirectX::XMVectorSubtract(target_next_pos, pos);   // ターゲットへ
            // 両方正規化
            vec_to_end = DirectX::XMVector3Normalize(vec_to_end);
            vec_to_target = DirectX::XMVector3Normalize(vec_to_target);

            // ほぼ同じベクトルになってしまった場合
            // 外積できないので、次のボーンに引き渡す
            auto cross = DirectX::XMVectorSubtract(vec_to_end, vec_to_target);
            if (DirectX::XMVector3Length(cross).m128_f32[0] <= epsilon) {
                continue;
            }

            // 外積計算及び角度計算
            auto cross_normalized = DirectX::XMVector3Normalize(cross);
            float angle = DirectX::XMVector3AngleBetweenNormals(vec_to_end, vec_to_target).m128_f32[0];

            // 回転限界を超えてしまったときは限界値に補正
            angle = std::min<float>(angle, ik_limit);
            DirectX::XMMATRIX rot = DirectX::XMMatrixRotationAxis(cross_normalized, angle);

            // 原点中心ではなく、pos中心に回転
            auto mat = DirectX::XMMatrixTranslationFromVector(-pos) *
                rot *
                DirectX::XMMatrixTranslationFromVector(pos);

            // 回転行列を保持しておく（乗算で回転重ね掛けを作っておく）
            mats[bidx] *= mat;

            // 対象となる点をすべて回転させる（現在の点から見て末端側を回転）
            for (auto idx = bidx - 1; idx >= 0; --idx) {
                bone_positions[idx] = DirectX::XMVector3Transform(bone_positions[idx], mat);
            }
            end_pos = DirectX::XMVector3Transform(end_pos, mat);

            // もし正解に近かったらループを抜ける
            if (DirectX::XMVector3Length(DirectX::XMVectorSubtract(end_pos, target_next_pos)).m128_f32[0] <= epsilon) {
                break;
            }
        }
    }

    // ボーン行列に反映
    int i = 0;
    for (auto cidx : ik.NodeIdxes) {
        m_BoneMetricesForMotion[cidx] = mats[i];
        ++i;
    }
    auto root_node = m_PMDData.GetBoneFromIndex(ik.NodeIdxes.back());
    RecursiveMatrixMultiply(&(root_node.value()), parent_mat);
}