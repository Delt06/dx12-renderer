#include "ShaderLibrary/GBufferUtils.hlsli"
#include "ShaderLibrary/ScreenParameters.hlsli"
#include "ShaderLibrary/Math.hlsli"
#include "ShaderLibrary/GBuffer.hlsli"
#include "ShaderLibrary/Pipeline.hlsli"

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

cbuffer ParametersCBuffer : register(b0)
{
    uint Steps;
    float Thickness;

    float2 TexelSize;
};

Texture2D sceneColor : register(t0);

float3 SamplePositionVS(float2 uv, out float out_zNDC)
{
    out_zNDC = gBufferDepth.SampleLevel(gBufferSampler, uv, 0);
    float3 positionNDC = ScreenSpaceUVToNDC(uv, out_zNDC);
    float3 positionVS = RestorePositionVS(positionNDC, g_Pipeline_InverseProjection).xyz;
    return positionVS;
}

float3 SamplePositionVS(float2 uv)
{
    float out_zNDC;
    return SamplePositionVS(uv, out_zNDC);
}

float3 SampleNormalVS(float2 uv)
{
    float3 normalWS = UnpackNormal(gBufferNormalsWS.Sample(gBufferSampler, uv).xyz);
    return normalize(mul((float3x3) g_Pipeline_View, normalWS));
}

float4 ToScreenSpaceUV(float3 positionVS)
{
    float4 positionCS = mul(g_Pipeline_Projection, float4(positionVS, 1.0));
    positionCS.xyz /= positionCS.w;

    positionCS.xy = positionCS.xy * 0.5 + 0.5;
    positionCS.y = 1 - positionCS.y;

    return positionCS;
}

float3 SampleSceneColor(float2 uv)
{
    return sceneColor.Sample(gBufferSampler, uv).rgb;
}

struct TraceOutput
{
    bool Hit;
    float2 UV;
    float Fade;
};

float3 hash33(float3 p3) // 3D input, 3D output
{
    p3 = frac(p3 * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return frac((p3.xxy + p3.yxx) * p3.zyx);
}


TraceOutput Trace(float2 uv, float roughness)
{
    TraceOutput output = (TraceOutput) 0;
    float gBufferDepth;
    float3 originVS = SamplePositionVS(uv, gBufferDepth);
    if (gBufferDepth > 0.9999)
    {
        return output;
    }
    float3 normalVS = SampleNormalVS(uv);

    float3 viewDir = normalize(originVS);
    float3 originWS = mul(g_Pipeline_InverseView, float4(originVS, 1.0)).xyz;
    float3 reflectDir = normalize(reflect(viewDir, normalVS) + hash33(originWS * 50) * 0.3 * roughness);

    const uint loops = 50;

    const float VdotR = dot(viewDir, reflectDir);
    const float fromCameraFade = smoothstep(1, 0.7, VdotR);
    const float toCameraFade = smoothstep(-1, -0.7, VdotR);
    output.Fade = fromCameraFade * toCameraFade;


    float step = 0.2;
    float3 rayOrigin = originVS + reflectDir * step;
    float totalDistance = step;
    float distanceThreshold = 0.05;
    bool hit1 = false;
    uint i;
    float zDelta = 0;

    for (i = 0; i < loops; ++i)
    {
        float4 currentUV = ToScreenSpaceUV(rayOrigin);
        float3 currentGBufferPositionVS = SamplePositionVS(currentUV.xy, gBufferDepth);

        zDelta = (rayOrigin.z - currentGBufferPositionVS.z);
        if (abs(zDelta) < distanceThreshold)
        {
            output.Hit = true;
            output.UV = currentUV.xy;
            return output;
        }

        if (zDelta > 0.01)
        {
            hit1 = true;
            break;
        }

        rayOrigin += reflectDir * step;
        totalDistance += step;
        step *= 1.01;
    }

    // binary search
    if (hit1)
    {
        for (i = 0; i < 5; ++i)
        {
            step *= 0.5;
            rayOrigin -= step * reflectDir * sign(zDelta);

            float4 currentUV = ToScreenSpaceUV(rayOrigin);
            float3 currentGBufferPositionVS = SamplePositionVS(currentUV.xy, gBufferDepth);

            zDelta = (rayOrigin.z - currentGBufferPositionVS.z);
            if (abs(zDelta) < distanceThreshold)
            {
                output.Hit = true;
                output.UV = currentUV.xy;
                return output;
            }
        }
    }


    return output;
}



float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    float4 surfaceSample = gBufferSurface.Sample(gBufferSampler, uv);
    float metallic, roughness, ao;
    UnpackSurface(surfaceSample, metallic, roughness, ao);


    TraceOutput traceOutput = Trace(uv, roughness);
    if (traceOutput.Hit)
    {
        float3 sceneColor = SampleSceneColor(traceOutput.UV);

        if (IsNaNAny(sceneColor))
            return 0;

        float fade = traceOutput.Fade;
        float2 centeredUV = abs(traceOutput.UV - 0.5);
        float2 uvFade = smoothstep(0.505, 0.5, centeredUV);
        fade *= uvFade.x * uvFade.y;

        return float4(sceneColor, fade);
    }

    return 0;
}
