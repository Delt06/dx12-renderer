#include "IBL_PreFilterEnvironment_CBuffer.hlsli"

struct VertexShaderInput
{
    float3 PositionOS : POSITION;
    float2 UV : TEXCOORD;
};

struct VertexShaderOutput
{
    float3 Normal : TEXCOORD;
    float4 PositionCS : SV_Position;
};

VertexShaderOutput main(VertexShaderInput IN)
{
    VertexShaderOutput OUT;
    OUT.PositionCS = float4(IN.PositionOS, 1.0);
    
    float2 uv = IN.UV;
    uv -= 0.5f; // [0, 1] -> [-0.5, 0.5]
    uv *= -1.0f; // flip the vertical coord
    
    float3 forward = parametersCB.Forward.xyz;
    float3 up = parametersCB.Up.xyz;
    float3 right = cross(forward, up);
    OUT.Normal = forward * 0.5f + up * uv.y + right * uv.x;

    return OUT;
}