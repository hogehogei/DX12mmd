#include "BasicShaderHeader.hlsli"
Texture2D<float4> tex : register(t0);   // 0�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`���[�i�e�N�X�`���j
Texture2D<float4> sph : register(t1);   // 1�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`���[�i��Z�X�t�B�A�}�b�v�j
Texture2D<float4> spa : register(t2);   // 2�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`���[�i���Z�X�t�B�A�}�b�v�j
Texture2D<float4> toon : register(t3);  // 3�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`���[�i�g�D�[���j
SamplerState smp : register(s0);        // 0�Ԗڂ̃T���v���[
SamplerState smpToon : register(s1);    // 1�Ԗڂ̃T���v���[�i�g�D�[���p�j

float4 BasicPS(VertexShaderOutput input) : SV_TARGET
{
	// ���̌������x�N�g���i���s�����j
	float3 light = normalize(float3(1, -1, 1));

	// ���C�g�̃J���[
	float lightColor = float3(1, 1, 1);

	// �f�B�t���[�Y�v�Z
	float diffuseB = saturate(dot(-light, input.normal));
	float4 toonDif = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	// ���̔��˃x�N�g��
	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	// �X�t�B�A�}�b�v�p
	float2 normalUV = (input.normal.xy + float2(1, -1)) * float2(0.5, -0.5);
	float2 sphereMapUV = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	float4 texColor = tex.Sample(smp, input.uv);

	
	return max(
		saturate(
			toonDif		    // �P�x
			* diffuse		// �f�B�t���[�Y�F
			* texColor		// �e�N�X�`���F
			* sph.Sample(smp, sphereMapUV) 			// �X�t�B�A�}�b�v�i��Z�j
		)
			+ 
		saturate(
			spa.Sample(smp, sphereMapUV) * texColor			// �X�t�B�A�}�b�v�i���Z�j
			+ float4(specularB * specular.rgb, 1)	// �X�y�L����
		)
			, float4(texColor * ambient, 1)			// �A���r�G���g
	);
}