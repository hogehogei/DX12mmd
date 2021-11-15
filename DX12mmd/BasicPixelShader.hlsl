#include "BasicShaderHeader.hlsli"
Texture2D<float4> tex : register(t0);   // 0番スロットに設定されたテクスチャー
SamplerState smp : register(s0);        // 0

float4 BasicPS(UVVertex input) : SV_TARGET
{
	return float4(tex.Sample(smp, input.uv));
}