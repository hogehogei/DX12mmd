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

	// ボーンの重みを正規化
	float bone_weight = weight / 100.0f;
	matrix bone_mat = (bones[boneno[0]] * bone_weight) + (bones[boneno[1]] * (1.0f - bone_weight));

	// ボーンを先に乗算
	pos = mul(bone_mat, pos);
	// シェーダーは列優先なので注意
	output.svpos = mul(mul(viewproj, world), pos);
	normal.w = 0;		// 重要。平行移動成分を無効にする

	output.normal = mul(world, normal);	          // 法線にもワールド行列を計算
	output.vnormal = mul(view, output.normal);
	output.uv = uv;

	output.ray = normalize(pos.xyz - eye);    // 視線ベクトルを計算

	return output;
}