#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <DirectXMath.h>

struct VMDMotion
{
public:

    char     BoneName[15];          // �{�[����
    uint32_t FrameNo;               // �t���[���ԍ�
    DirectX::XMFLOAT3 Location;     // �ʒu
    DirectX::XMFLOAT4 Quaternion;   // ��]�N�H�[�^�j�I��
    uint8_t  Bezier[64];            // [4][4][4] �x�W�F��ԃp�����[�^
};

struct MotionKeyFrame
{
public:

    uint32_t FrameNo;               // �A�j���[�V�����J�n����̃t���[���ԍ�
    DirectX::XMVECTOR Quaternion;   // �N�H�[�^�j�I��


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

    uint32_t     m_MotionDataNum;       // ���[�V�����f�[�^��
    MotionTable  m_MotionList;          // ���[�V�������X�g [�{�[����, �L�[�t���[�����X�g]�̘A�z�z��
    uint32_t     m_MaxKeyFrameNo;       // �ő�L�[�t���[���ԍ�
};