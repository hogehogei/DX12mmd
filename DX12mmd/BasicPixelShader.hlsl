#include "BasicShaderHeader.hlsli"
Texture2D<float4> tex : register(t0);   // 0�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`���[
SamplerState smp : register(s0);        // 0

float4 BasicPS(UVVertex input) : SV_TARGET
{
	return float4(tex.Sample(smp, input.uv));
}