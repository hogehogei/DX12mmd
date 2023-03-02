#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <DirectXMath.h>

struct VMDMotion
{
public:

    char     BoneName[15];          // ボーン名
    uint32_t FrameNo;               // フレーム番号
    DirectX::XMFLOAT3 Location;     // 位置
    DirectX::XMFLOAT4 Quaternion;   // 回転クォータニオン
    uint8_t  Bezier[64];            // [4][4][4] ベジェ補間パラメータ
};

struct MotionKeyFrame
{
public:

    uint32_t FrameNo;               // アニメーション開始からのフレーム番号
    DirectX::XMVECTOR Quaternion;   // クォータニオン


    MotionKeyFrame(uint32_t frame_no, DirectX::XMVECTOR q);
};

class VMDMotionTable
{
public:

    struct MotionInterpolater
    {
    public:

        DirectX::XMMATRIX Slerp(uint32_t frame_no) const
        {
            if (Begin.FrameNo == End.FrameNo) {
                return DirectX::XMMatrixRotationQuaternion(Begin.Quaternion);
            }

            float t = static_cast<float>(frame_no - Begin.FrameNo) / static_cast<float>(End.FrameNo - Begin.FrameNo);
#if 0
            DirectX::XMMATRIX rotation =
                DirectX::XMMatrixRotationQuaternion(Begin.Quaternion) * (1.0f - t) +
                DirectX::XMMatrixRotationQuaternion(End.Quaternion)   * t;
#endif

            DirectX::XMMATRIX rotation =
                DirectX::XMMatrixRotationQuaternion(
                    DirectX::XMQuaternionSlerp(Begin.Quaternion, End.Quaternion, t)
                );

            return rotation;
        }

        std::string    Name;
        MotionKeyFrame Begin;
        MotionKeyFrame End;
    };

    typedef std::unordered_map <std::string, std::vector<MotionKeyFrame>> MotionTable;
    typedef std::shared_ptr<std::vector<MotionInterpolater>> NowMotionListPtr;

public:
    VMDMotionTable();

    bool Open(const std::filesystem::path& filename);

    const MotionTable& GetMotionTable() const;
    NowMotionListPtr   GetNowMotionList(uint32_t frame_no);
    uint32_t MaxKeyFrameNo() const;
    
private:

    uint32_t     m_MotionDataNum;       // モーションデータ数
    MotionTable  m_MotionList;          // モーションリスト [ボーン名, キーフレームリスト]の連想配列
    uint32_t     m_MaxKeyFrameNo;       // 最大キーフレーム番号
};