#include "ShaderLibrary/Model.hlsli"

float4 main(float3 positionOs : POSITION) : SV_POSITION
{
	return mul(g_Model_ModelViewProjection, float4(positionOs, 1.0f));
}
