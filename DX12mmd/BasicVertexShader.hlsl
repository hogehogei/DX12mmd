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

	// シェーダーは列優先なので注意
	output.svpos = mul(mul(viewproj, world), pos);
	normal.w = 0;		// 重要。平行移動成分を無効にする

	output.normal = mul(world, normal);	// 法線にもワールド行列を計算
	output.uv = uv;

	return output;
}