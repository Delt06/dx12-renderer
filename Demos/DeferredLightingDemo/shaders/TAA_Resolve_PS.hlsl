#include <ShaderLibrary/Common/RootSignature.hlsli>
#include <ShaderLibrary/Pipeline.hlsli>

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

Texture2D currentColorBuffer : register(t0);
Texture2D historyColorBuffer : register(t1);
Texture2D velocityColorBuffer : register(t2);

float3 SampleColorBuffer(float2 uv)
{
    return currentColorBuffer.Sample(g_Common_PointClampSampler, uv).rgb;
}

float3 SampleColorBufferOffset(float2 uv, int2 offset)
{
    return currentColorBuffer.Sample(g_Common_PointClampSampler, uv + offset * g_Pipeline_Screen_TexelSize).rgb;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    // https://sugulee.wordpress.com/2021/06/21/temporal-anti-aliasingtaa-tutorial/
    const float2 uv = IN.UV;
    float2 velocity = velocityColorBuffer.Sample(g_Common_PointClampSampler, uv).xy;
    float2 previousPixelUv = uv - velocity;
    
    float3 currentColor = SampleColorBuffer(uv);
    float3 historyColor = historyColorBuffer.Sample(g_Common_LinearClampSampler, previousPixelUv).rgb;
    
    float3 nearColor0 = SampleColorBufferOffset(uv, int2(1, 0));
    float3 nearColor1 = SampleColorBufferOffset(uv, int2(0, 1));
    float3 nearColor2 = SampleColorBufferOffset(uv, int2(-1, 0));
    float3 nearColor3 = SampleColorBufferOffset(uv, int2(0, -1));
    
    float3 boxMin = min(currentColor, min(nearColor0, min(nearColor1, min(nearColor2, nearColor3))));
    float3 boxMax = max(currentColor, max(nearColor0, max(nearColor1, max(nearColor2, nearColor3))));
    
    historyColor = clamp(historyColor, boxMin, boxMax);
    
    float modulationFactor = 0.9f;
    
    float3 resultColor = lerp(currentColor, historyColor, modulationFactor);
    return float4(resultColor, 1.0);
}