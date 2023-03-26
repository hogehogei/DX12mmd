#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <optional>

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
    DirectX::XMFLOAT4 Quaternion;   // �N�H�[�^�j�I��

    DirectX::XMFLOAT3 Offset;               // IK�̏������W����̃I�t�Z�b�g���
    DirectX::XMFLOAT2 RotateBezierP1;       // ��]��ԃx�W�F����_p1
    DirectX::XMFLOAT2 RotateBezierP2;       // ��]��ԃx�W�F����_p2

    MotionKeyFrame(
        uint32_t frame_no, 
        const DirectX::XMFLOAT4& q,
        const DirectX::XMFLOAT3& offset,
        const DirectX::XMFLOAT2& p1, 
        const DirectX::XMFLOAT2& p2
    );
};

#pragma pack(1)
// �\��f�[�^�i���_���[�t�f�[�^�j
struct VMDMorph
{
    char Name[15];      // ���O
    uint32_t FrameNo;   // �t���[���ԍ�
    float Weight;       // �E�F�C�g(0.0f �` 1.0f)
};
#pragma pack()

#pragma pack(1)
// �J����
struct VMDCamera
{
    uint32_t FrameNo;               // �t���[���ԍ�
    float Distance;                 // ����
    DirectX::XMFLOAT3 Pos;          // ���W
    DirectX::XMFLOAT3 EulerAngle;   // �I�C���[�p
    uint8_t Interpolation[24];      // ���
    uint32_t FoV;                   // ���E�p
    uint8_t PersFlg;                // �p�[�X�t���O ON/OFF
};
#pragma pack()

// ���C�g�Ɩ��f�[�^
struct VMDLight
{
    uint32_t FrameNo;               // �t���[���ԍ�
    DirectX::XMFLOAT3 LightRGB;     // ���C�g�F
    DirectX::XMFLOAT3 Vector;       // �����x�N�g���i���s�����j
};

#pragma pack(1)
// �Z���t�e�f�[�^
struct VMDSelfShadow
{
    uint32_t FrameNo;       // �t���[���ԍ�
    uint8_t  Mode;          // �e���[�h�i0:�e����, 1:���[�h1, 2:���[�h2�j
    float    Distance;      // ����
};
#pragma pack()

// IK ON/OFF�f�[�^
struct VMDIKEnable
{
    uint32_t FrameNo;       // �L�[�t���[��������ԍ�
                            // ���O��ON/OFF�t���O�̃}�b�v
    std::unordered_map<std::string, bool> IkEnableTable;
};

class VMDMotionTable
{
public:

    struct MotionInterpolater
    {
    public:

        double GetYFromXOnBezier(double x, const DirectX::XMFLOAT2& p1, const DirectX::XMFLOAT2& p2, uint8_t count) const;
        void Slerp(uint32_t frame_no, DirectX::XMMATRIX* rotate_mat_out, DirectX::XMVECTOR* offset_out) const;

        std::string    Name;
        MotionKeyFrame Begin;
        MotionKeyFrame End;
    };

    typedef std::unordered_map <std::string, std::vector<MotionKeyFrame>> MotionTable;
    typedef std::shared_ptr<std::vector<MotionInterpolater>> NowMotionListPtr;
    typedef std::optional<VMDIKEnable> VMDIKEnableResult;

public:
    VMDMotionTable();

    bool Open(const std::filesystem::path& filename);

    const MotionTable& GetMotionTable() const;
    NowMotionListPtr  GetNowMotionList(uint32_t frame_no);
    VMDIKEnableResult GetIKEnable(uint32_t frame_no) const;
    
    uint32_t MaxKeyFrameNo() const;
    
private:

    uint32_t     m_MotionDataNum;       // ���[�V�����f�[�^��
    MotionTable  m_MotionList;          // ���[�V�������X�g [�{�[����, �L�[�t���[�����X�g]�̘A�z�z��
    uint32_t     m_MaxKeyFrameNo;       // �ő�L�[�t���[���ԍ�

    uint32_t     m_MorphDataNum;        // ���[�t�f�[�^��
    std::vector<VMDMorph> m_MorphList;  // ���[�t�f�[�^

    uint32_t     m_CameraDataNum;                     // �J�����f�[�^��
    std::vector<VMDCamera> m_CameraList;              // �J�����f�[�^

    uint32_t     m_LightDataNum;                      // �Ɩ��f�[�^��
    std::vector<VMDLight> m_LightList;                // �Ɩ��f�[�^

    uint32_t     m_SelfShadowDataNum;                 // �Z���t�V���h�E�� 
    std::vector<VMDSelfShadow> m_SelfShadowList;      // �Z���t�V���h�E�f�[�^

    uint32_t     m_IKSwitchNum;                       // IK�؂�ւ��f�[�^��
    std::vector<VMDIKEnable>   m_IKSwitchList;        // IK�؂�ւ��f�[�^
};