#include "VMD.hpp"

#include <fstream>
#include <algorithm>

MotionKeyFrame::MotionKeyFrame(
    uint32_t frame_no, 
    const DirectX::XMFLOAT4& q, 
    const DirectX::XMFLOAT3& offset,
    const DirectX::XMFLOAT2& p1, 
    const DirectX::XMFLOAT2& p2
)
    :
    FrameNo(frame_no),
    Quaternion(q),
    Offset(offset),
    RotateBezierP1(p1),
    RotateBezierP2(p2)
{}

VMDMotionTable::VMDMotionTable()
    :
    m_MotionDataNum(0),
    m_MotionList()
{}

double VMDMotionTable::MotionInterpolater::GetYFromXOnBezier(double x, const DirectX::XMFLOAT2& p1, const DirectX::XMFLOAT2& p2, uint8_t count) const
{
    // ベジェ曲線が直線補間なら計算不要とする。
    constexpr double e = std::numeric_limits<double>::epsilon();
    if ((std::abs(p1.x - p1.y) < e) && (std::abs(p2.x - p2.y) < e)) {
        return x;
    }

    double t = x;
    const float k0 = 1.0 + (3.0 * p1.x) - (3.0 * p2.x);
    const float k1 = (3.0 * p2.x) - (6.0 * p1.x);
    const float k2 = 3.0 * p1.x;

    // 判定打切り誤差
    constexpr double stop_eps = 0.0005;

    // x から tを近似で求める
    for (int i = 0; i < count; ++i) {
        // f(t)を求める
        double ft = (k0 * t * t * t) + (k1 * t * t) + (k2 * t) - x;
        // 結果が判定打切り誤差内だったら打ち切る
        if (ft < stop_eps && ft > stop_eps) {
            break;
        }

        t -= ft / 2;
    }

    double r = 1 - t;
    return (t * t * t) + (3 * t * t * r * p2.y) + (3 * t * r * r * p1.y);
}

void VMDMotionTable::MotionInterpolater::Slerp(
    uint32_t frame_no, DirectX::XMMATRIX* rotate_mat_out, DirectX::XMVECTOR* offset_out ) const
{
    DirectX::XMVECTOR begin_offset = DirectX::XMLoadFloat3(&(Begin.Offset));
    if (Begin.FrameNo == End.FrameNo) {
        *rotate_mat_out = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&Begin.Quaternion));
        *offset_out = begin_offset;
    }
    else {
        float t = static_cast<float>(frame_no - Begin.FrameNo) / static_cast<float>(End.FrameNo - Begin.FrameNo);
        t = GetYFromXOnBezier(t, Begin.RotateBezierP1, Begin.RotateBezierP2, 12);

        DirectX::XMMATRIX rotation =
            DirectX::XMMatrixRotationQuaternion(
                DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&Begin.Quaternion), DirectX::XMLoadFloat4(&End.Quaternion), t)
            );

        *rotate_mat_out = rotation;
        *offset_out = DirectX::XMVectorLerp(begin_offset, DirectX::XMLoadFloat3(&(End.Offset)), t);
    }
}

bool VMDMotionTable::Open(const std::filesystem::path& filename)
{
    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    if (!ifs) {
        return false;
    }

    // 先頭50バイトは読み飛ばし
    ifs.seekg(50, std::ios::beg);
    // モーションデータ数読み込み
    ifs.read(reinterpret_cast<char*>(&m_MotionDataNum), sizeof(m_MotionDataNum));

    std::vector<VMDMotion> vmd_motion_data(m_MotionDataNum);
    for (auto& motion : vmd_motion_data) {
        ifs.read(reinterpret_cast<char*>(&motion.BoneName), sizeof(motion.BoneName));
        ifs.read(
            reinterpret_cast<char*>(&motion.FrameNo),
            sizeof(motion.FrameNo) +
            sizeof(motion.Location) +
            sizeof(motion.Quaternion) +
            sizeof(motion.Bezier)
        );
    }

    // VMDのモーションデータを、実際に使用するモーションテーブルへ変換
    for (auto& vmd_motion : vmd_motion_data) {
        m_MotionList[vmd_motion.BoneName].emplace_back(
            MotionKeyFrame(
                vmd_motion.FrameNo, 
                vmd_motion.Quaternion,
                DirectX::XMFLOAT3(vmd_motion.Location),
                DirectX::XMFLOAT2((vmd_motion.Bezier[3] / 127.0f), (vmd_motion.Bezier[7] / 127.0f)),
                DirectX::XMFLOAT2((vmd_motion.Bezier[11] / 127.0f), (vmd_motion.Bezier[15] / 127.0f))
            )
        );

        // 最大キーフレーム番号をメモ
        m_MaxKeyFrameNo = std::max(m_MaxKeyFrameNo, vmd_motion.FrameNo);
    }

    // キーフレーム順に昇順ソート
    for (auto& keyframes : m_MotionList) {
        auto& keyframelist = keyframes.second;
        std::sort(
            keyframelist.begin(), keyframelist.end(),
            [](const MotionKeyFrame& a, const MotionKeyFrame& b)
            {
                return a.FrameNo < b.FrameNo;
            }
        );
    }

    // 表情データ
    ifs.read(reinterpret_cast<char*>(&m_MorphDataNum), sizeof(m_MorphDataNum));
    m_MorphList.resize(m_MorphDataNum);
    ifs.read(reinterpret_cast<char*>(m_MorphList.data()), sizeof(VMDMorph) * m_MorphDataNum);

    // カメラ
    ifs.read(reinterpret_cast<char*>(&m_CameraDataNum), sizeof(m_CameraDataNum));
    m_CameraList.resize(m_CameraDataNum);
    ifs.read(reinterpret_cast<char*>(m_CameraList.data()), sizeof(VMDCamera) * m_CameraDataNum);

    // 照明データ
    ifs.read(reinterpret_cast<char*>(&m_LightDataNum), sizeof(m_LightDataNum));
    m_LightList.resize(m_LightDataNum);
    ifs.read(reinterpret_cast<char*>(m_LightList.data()), sizeof(VMDLight) * m_LightDataNum);

    // セルフシャドウデータ
    ifs.read(reinterpret_cast<char*>(&m_SelfShadowDataNum), sizeof(m_SelfShadowDataNum));
    m_SelfShadowList.resize(m_SelfShadowDataNum);
    ifs.read(reinterpret_cast<char*>(m_SelfShadowList.data()), sizeof(VMDSelfShadow) * m_SelfShadowDataNum);

    // IK切り替えデータ
    ifs.read(reinterpret_cast<char*>(&m_IKSwitchNum), sizeof(m_IKSwitchNum));
    m_IKSwitchList.resize(m_IKSwitchNum);

    for (auto& ik_enable : m_IKSwitchList) {
        // キーフレーム番号読み込み
        ifs.read(reinterpret_cast<char*>(&ik_enable.FrameNo), sizeof(ik_enable.FrameNo));

        // 可視フラグ 使わないので読み飛ばす
        uint8_t is_visible = 0;
        ifs.read(reinterpret_cast<char*>(&is_visible), sizeof(is_visible));

        // 対象ボーン数読み込み
        uint32_t ik_bone_count = 0;
        ifs.read(reinterpret_cast<char*>(&ik_bone_count), sizeof(ik_bone_count));
        
        // ON/OFF情報読み込み
        for (int i = 0; i < ik_bone_count; ++i) {
            char name[20];
            ifs.read(reinterpret_cast<char*>(&name), sizeof(name));
            std::string s(name);

            uint8_t enable_flg = 0;
            ifs.read(reinterpret_cast<char*>(&enable_flg), sizeof(enable_flg));
            ik_enable.IkEnableTable[s] = enable_flg;
        }
    }

    std::sort(
        m_IKSwitchList.begin(), m_IKSwitchList.end(),
        [](const VMDIKEnable& a, const VMDIKEnable& b) {
            return a.FrameNo < b.FrameNo;
        }
    );


    return true;
}

const VMDMotionTable::MotionTable& VMDMotionTable::GetMotionTable() const
{
    return m_MotionList;
}

VMDMotionTable::NowMotionListPtr VMDMotionTable::GetNowMotionList(uint32_t frame_no)
{
    NowMotionListPtr motion_ptr = std::make_shared<std::vector<MotionInterpolater>>();

    for (const auto& bone_motion : m_MotionList) {
        // 合致するものを探す
        auto motions = bone_motion.second;
        auto ritr = std::find_if(
            motions.rbegin(), motions.rend(),
            [frame_no](const MotionKeyFrame& m) {
                return m.FrameNo <= frame_no;
            }
        );

        if (ritr == motions.rend()) {
            continue;
        }
        

        MotionInterpolater interpolater = {
            bone_motion.first,
            *ritr,
            ritr.base() == motions.end() ? *ritr : *(ritr.base())
        };

        motion_ptr->emplace_back(interpolater);
    }

    return motion_ptr;
}

VMDMotionTable::VMDIKEnableResult VMDMotionTable::GetIKEnable(uint32_t frame_no) const
{
    auto itr = std::find_if(
        m_IKSwitchList.rbegin(),
        m_IKSwitchList.rend(),
        [frame_no](const VMDIKEnable& ike) {
            return ike.FrameNo <= frame_no;
        }
    );

    return itr == m_IKSwitchList.rend() ? std::nullopt : VMDIKEnableResult(*itr);
}

uint32_t VMDMotionTable::MaxKeyFrameNo() const
{
    return m_MaxKeyFrameNo;
}