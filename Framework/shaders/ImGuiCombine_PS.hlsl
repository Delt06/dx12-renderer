#include <ShaderLibrary/Common/RootSignature.hlsli>

Texture2D source : register(t0);

float4 main(const float2 uv : TEXCOORD) : SV_Target
{
    return pow(source.SampleLevel(g_Common_LinearClampSampler, uv, 0), 2.2);
}
