#include "VMD.hpp"

#include <fstream>

MotionKeyFrame::MotionKeyFrame(uint32_t frame_no, DirectX::XMVECTOR q)
    :
    FrameNo(frame_no),
    Quaternion(q)
{}

VMDMotionTable::VMDMotionTable()
    :
    m_MotionDataNum(0),
    m_MotionList()
{}

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
                vmd_motion.FrameNo, DirectX::XMLoadFloat4(&vmd_motion.Quaternion)
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

uint32_t VMDMotionTable::MaxKeyFrameNo() const
{
    return m_MaxKeyFrameNo;
}