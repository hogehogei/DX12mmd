// ���_�V�F�[�_����s�N�Z���V�F�[�_�ւ̂����Ɏg�p����\����

struct VertexShaderOutput
{
    float4 svpos  :  SV_POSITION;     // �V�X�e���p���_���W
    float4 pos  :    POSITION;        // ���_���W
    float4 normal :  NORMAL0;         // �@���x�N�g��
    float4 vnormal : NORMAL1;         // �r���[�ϊ���̖@���x�N�g��
    float2 uv     :  TEXCOORD;        // uv�l
    float3 ray : VECTOR;              // �����x�N�g��
};

// �萔�o�b�t�@�[
cbuffer cbuff0 : register(b0)
{
    matrix world;       // ���[���h�s��
    matrix view;        // �r���[�s��
    matrix proj;        // �v���W�F�N�V�����s��
    float3 eye;         // ���_���W
};

cbuffer Material : register(b1)
{
    float4 diffuse;     // �f�B�t���[�Y�F
    float4 specular;    // �X�y�L����
    float3 ambient;     // �A���r�G���g
};
