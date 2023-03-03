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

    DirectX::XMFLOAT2 RotateBezierP1;       // ��]��ԃx�W�F����_p1
    DirectX::XMFLOAT2 RotateBezierP2;       // ��]��ԃx�W�F����_p2

    MotionKeyFrame(uint32_t frame_no, const DirectX::XMVECTOR& q, const DirectX::XMFLOAT2& p1, const DirectX::XMFLOAT2& p2);
};

class VMDMotionTable
{
public:

    struct MotionInterpolater
    {
    public:

        double GetYFromXOnBezier(double x, const DirectX::XMFLOAT2& p1, const DirectX::XMFLOAT2& p2, uint8_t count) const;
        DirectX::XMMATRIX Slerp(uint32_t frame_no) const;

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