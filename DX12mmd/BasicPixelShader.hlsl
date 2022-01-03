#include "BasicShaderHeader.hlsli"
Texture2D<float4> tex : register(t0);   // 0番スロットに設定されたテクスチャー（テクスチャ）
Texture2D<float4> sph : register(t1);   // 1番スロットに設定されたテクスチャー（乗算スフィアマップ）
Texture2D<float4> spa : register(t2);   // 2番スロットに設定されたテクスチャー（加算スフィアマップ）
Texture2D<float4> toon : register(t3);  // 3番スロットに設定されたテクスチャー（トゥーン）
SamplerState smp : register(s0);        // 0番目のサンプラー
SamplerState smpToon : register(s1);    // 1番目のサンプラー（トゥーン用）

float4 BasicPS(VertexShaderOutput input) : SV_TARGET
{
	// 光の向かうベクトル（平行光源）
	float3 light = normalize(float3(1, -1, 1));

	// ライトのカラー
	float lightColor = float3(1, 1, 1);

	// ディフューズ計算
	float diffuseB = saturate(dot(-light, input.normal));
	float4 toonDif = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	// 光の反射ベクトル
	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	// スフィアマップ用
	float2 normalUV = (input.normal.xy + float2(1, -1)) * float2(0.5, -0.5);
	float2 sphereMapUV = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	float4 texColor = tex.Sample(smp, input.uv);

	
	return max(
		saturate(
			toonDif		    // 輝度
			* diffuse		// ディフューズ色
			* texColor		// テクスチャ色
			* sph.Sample(smp, sphereMapUV) 			// スフィアマップ（乗算）
		)
			+ 
		saturate(
			spa.Sample(smp, sphereMapUV) * texColor			// スフィアマップ（加算）
			+ float4(specularB * specular.rgb, 1)	// スペキュラ
		)
			, float4(texColor * ambient, 1)			// アンビエント
	);
}