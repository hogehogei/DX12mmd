#include "BasicShaderHeader.hlsli"

UVVertex BasicVS( float4 pos : POSITION, float2 uv : TEXCOORD )
{
	UVVertex output;

	output.svpos = mul(mat, pos);
	output.uv = uv;

	return output;
}