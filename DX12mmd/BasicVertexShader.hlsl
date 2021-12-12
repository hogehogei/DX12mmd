#include "BasicShaderHeader.hlsli"

UVVertex BasicVS( 
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 boneno : BONE_NO,
	min16uint weight : WEIGHT
)
{
	UVVertex output;

	// �V�F�[�_�[�͗�D��Ȃ̂Œ���
	output.svpos = mul(mul(viewproj, world), pos);
	normal.w = 0;		// �d�v�B���s�ړ������𖳌��ɂ���

	output.normal = mul(world, normal);	// �@���ɂ����[���h�s����v�Z
	output.uv = uv;

	return output;
}