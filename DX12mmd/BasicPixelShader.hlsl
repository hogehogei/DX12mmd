#include "BasicShaderHeader.hlsli"
Texture2D<float4> tex : register(t0);   // 0番スロットに設定されたテクスチャー
SamplerState smp : register(s0);        // 0

float4 BasicPS(UVVertex input) : SV_TARGET
{
	float3 light = normalize(float3(-1, 1, -1));
	float brightness = dot(light, input.normal);

	//return float4(tex.Sample(smp, input.uv));
	return float4(brightness, brightness, brightness, 1);
}