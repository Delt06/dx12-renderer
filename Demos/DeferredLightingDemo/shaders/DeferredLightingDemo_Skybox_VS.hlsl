#include "ShaderLibrary/Model.hlsli"

struct VertexShaderOutput
{
    float3 CubemapUV : CUBEMAP_UV;
    float4 PositionCS : SV_Position;
};

VertexShaderOutput main(float3 pos : POSITION)
{
    VertexShaderOutput OUT;
    
    OUT.CubemapUV = pos;
    
    float4 positionCS = mul(g_Model_ModelViewProjection, float4(pos, 1.0));
    positionCS.z = 0.9999 * positionCS.w;
    OUT.PositionCS = positionCS;
	
    return OUT;
}