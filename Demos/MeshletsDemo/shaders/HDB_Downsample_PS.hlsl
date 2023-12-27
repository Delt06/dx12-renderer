#include <ShaderLibrary/Common/RootSignature.hlsli>

Texture2D<float> _SourceDepthMip : register(t0);

float main(float2 uv : TEXCOORD) : SV_Depth
{
    const float4 depths = _SourceDepthMip.Gather(g_Common_LinearClampSampler, uv);
    return max(depths.x, max(depths.y, max(depths.z, depths.w)));
}
