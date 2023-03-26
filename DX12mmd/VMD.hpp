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
    DirectX::XMFLOAT4 Quaternion;   // クォータニオン

    DirectX::XMFLOAT3 Offset;               // IKの初期座標からのオフセット情報
    DirectX::XMFLOAT2 RotateBezierP1;       // 回転補間ベジェ制御点p1
    DirectX::XMFLOAT2 RotateBezierP2;       // 回転補間ベジェ制御点p2

    MotionKeyFrame(
        uint32_t frame_no, 
        const DirectX::XMFLOAT4& q,
        const DirectX::XMFLOAT3& offset,
        const DirectX::XMFLOAT2& p1, 
        const DirectX::XMFLOAT2& p2
    );
};

#pragma pack(1)
// 表情データ（頂点モーフデータ）
struct VMDMorph
{
    char Name[15];      // 名前
    uint32_t FrameNo;   // フレーム番号
    float Weight;       // ウェイト(0.0f ～ 1.0f)
};
#pragma pack()

#pragma pack(1)
// カメラ
struct VMDCamera
{
    uint32_t FrameNo;               // フレーム番号
    float Distance;                 // 距離
    DirectX::XMFLOAT3 Pos;          // 座標
    DirectX::XMFLOAT3 EulerAngle;   // オイラー角
    uint8_t Interpolation[24];      // 補間
    uint32_t FoV;                   // 視界角
    uint8_t PersFlg;                // パースフラグ ON/OFF
};
#pragma pack()

// ライト照明データ
struct VMDLight
{
    uint32_t FrameNo;               // フレーム番号
    DirectX::XMFLOAT3 LightRGB;     // ライト色
    DirectX::XMFLOAT3 Vector;       // 光線ベクトル（平行光源）
};

#pragma pack(1)
// セルフ影データ
struct VMDSelfShadow
{
    uint32_t FrameNo;       // フレーム番号
    uint8_t  Mode;          // 影モード（0:影無し, 1:モード1, 2:モード2）
    float    Distance;      // 距離
};
#pragma pack()

// IK ON/OFFデータ
struct VMDIKEnable
{
    uint32_t FrameNo;       // キーフレームがある番号
                            // 名前とON/OFFフラグのマップ
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

    uint32_t     m_MotionDataNum;       // モーションデータ数
    MotionTable  m_MotionList;          // モーションリスト [ボーン名, キーフレームリスト]の連想配列
    uint32_t     m_MaxKeyFrameNo;       // 最大キーフレーム番号

    uint32_t     m_MorphDataNum;        // モーフデータ数
    std::vector<VMDMorph> m_MorphList;  // モーフデータ

    uint32_t     m_CameraDataNum;                     // カメラデータ数
    std::vector<VMDCamera> m_CameraList;              // カメラデータ

    uint32_t     m_LightDataNum;                      // 照明データ数
    std::vector<VMDLight> m_LightList;                // 照明データ

    uint32_t     m_SelfShadowDataNum;                 // セルフシャドウ数 
    std::vector<VMDSelfShadow> m_SelfShadowList;      // セルフシャドウデータ

    uint32_t     m_IKSwitchNum;                       // IK切り替えデータ数
    std::vector<VMDIKEnable>   m_IKSwitchList;        // IK切り替えデータ
};