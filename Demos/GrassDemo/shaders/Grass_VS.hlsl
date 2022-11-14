#include <ShaderLibrary/Common/RootSignature.hlsli>

#include "ShaderLibrary/Pipeline.hlsli"
#include "GrassModel.hlsli"
#include "GrassConstants.hlsli"
#include "GrassVaryings.hlsli"

StructuredBuffer<GrassModel> _GrassModelsSB : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
Texture2D<float> _WindNoise : register(t2, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

struct VertexAttributes
{
    float3 PositionOS : POSITION;
    float2 UV : TEXCOORD;
};

float3 RotateAboutAxisRadians(const float3 value, float3 axis, float rotation)
{
    float s, c;
    sincos(rotation, s, c);
    const float oneMinusC = 1.0 - c;

    axis = normalize(axis);
    const float3x3 rotationMatrix =
    {
        oneMinusC * axis.x * axis.x + c, oneMinusC * axis.x * axis.y - axis.z * s, oneMinusC * axis.z * axis.x + axis.y * s,
        oneMinusC * axis.x * axis.y + axis.z * s, oneMinusC * axis.y * axis.y + c, oneMinusC * axis.y * axis.z - axis.x * s,
        oneMinusC * axis.z * axis.x - axis.y * s, oneMinusC * axis.y * axis.z + axis.x * s, oneMinusC * axis.z * axis.z + c
    };
    return mul(transpose(rotationMatrix), value);
}

inline float4 TransformObjectToWorldPosition(float3 positionOS)
{
    return mul(_GrassModelsSB[_Index].Model, float4(positionOS, 1.0));
}

float4 VertexAnimationWS(float3 positionOS, float2 uv, float time)
{
    const float4 positionWS = TransformObjectToWorldPosition(positionOS);
    const float2 windUV = positionWS.xz;// * _WindNoise_ST.xy + _WindNoise_ST.zw;
    half wind = _WindNoise.SampleLevel(g_Common_LinearWrapSampler, windUV, 0);
    wind *= 10;//;_WindRandomization;
    wind += time;
    wind *= 2;//_WindSpeed;
    wind = sin(wind);

    wind *= 1 - uv.y;
    wind *= 0.05f;//_WindAmplitude;
    positionOS = RotateAboutAxisRadians(positionOS, float3(0, 0, 1), wind);

    return TransformObjectToWorldPosition(positionOS);
}

Varyings main(VertexAttributes IN)
{
    Varyings OUT;

    const float3 positionOS = IN.PositionOS;
    const float2 uv = IN.UV;
    const float4 positionWS = VertexAnimationWS(positionOS, uv, g_Pipeline_Time);
    const float4 positionCS = mul(g_Pipeline_ViewProjection, positionWS);
    float4 jitterPositionCS = positionCS;
    jitterPositionCS.xy += g_Pipeline_TAA_JitterOffset * positionCS.w;
    OUT.PositionCS = jitterPositionCS;

    OUT.CurrentPositionCS = positionCS;

    const float4 previousPositionWS = VertexAnimationWS(positionOS, uv, g_Pipeline_Time - g_Pipeline_DeltaTime);
    OUT.PreviousPositionCS = mul(g_Pipeline_TAA_PreviousViewProjection, previousPositionWS);

    OUT.UV = uv;

    return OUT;
}
