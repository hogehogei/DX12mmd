#include "BasicShaderHeader.hlsli"

VertexShaderOutput BasicVS(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 boneno : BONE_NO,
	min16uint weight : WEIGHT
)
{
	VertexShaderOutput output;
	matrix viewproj = mul(proj, view);

	// �{�[���̏d�݂𐳋K��
	float bone_weight = weight / 100.0f;
	matrix bone_mat = (bones[boneno[0]] * bone_weight) + (bones[boneno[1]] * (1.0f - bone_weight));

	// �{�[�����ɏ�Z
	pos = mul(bone_mat, pos);
	// �V�F�[�_�[�͗�D��Ȃ̂Œ���
	output.svpos = mul(mul(viewproj, world), pos);
	normal.w = 0;		// �d�v�B���s�ړ������𖳌��ɂ���

	output.normal = mul(world, normal);	          // �@���ɂ����[���h�s����v�Z
	output.vnormal = mul(view, output.normal);
	output.uv = uv;

	output.ray = normalize(pos.xyz - eye);    // �����x�N�g�����v�Z

	return output;
}