// ���_�V�F�[�_����s�N�Z���V�F�[�_�ւ̂����Ɏg�p����\����

struct UVVertex
{
    float4 svpos : SV_POSITION;     // �V�X�e���p���_���W
    float4 normal: NORMAL;          // �@���x�N�g��
    float2 uv    : TEXCOORD;        // uv�l
};

// �萔�o�b�t�@�[
cbuffer cbuff0 : register(b0)
{
    matrix world;       // ���[���h�s��
    matrix viewproj;    // �r���[�v���W�F�N�V�����s��
};