#include <ShaderLibrary/GBufferUtils.hlsli>
#include <ShaderLibrary/ScreenParameters.hlsli>

struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

cbuffer ParametersCBuffer : register(b0)
{
    matrix InverseProjection;
    matrix InverseView;
    matrix View;
    matrix Projection;
    
    uint Steps;
    float Thickness;
    float2 JitterOffset;
    
    float2 TexelSize;
    float2 _Padding;
};

Texture2D sceneColor : register(t0);
Texture2D normals : register(t1);
Texture2D surface : register(t2);
Texture2D depth : register(t3);

SamplerState defaultSampler : register(s0);

float3 SamplePositionVS(float2 uv, out float out_zNDC)
{
    out_zNDC = depth.SampleLevel(defaultSampler, uv, 0).x;
    float3 positionNDC = ScreenSpaceUVToNDC(uv, out_zNDC);
    float3 positionVS = RestorePositionVS(positionNDC, InverseProjection).xyz;
    return positionVS;
}

float3 SamplePositionVS(float2 uv)
{
    float out_zNDC;
    return SamplePositionVS(uv, out_zNDC);
}

float3 SampleNormalVS(float2 uv)
{
    float3 normalWS = UnpackNormal(normals.Sample(defaultSampler, uv).xyz);
    return normalize(mul((float3x3) View, normalWS));
}

float4 ToScreenSpaceUV(float3 positionVS)
{
    float4 positionCS = mul(Projection, float4(positionVS, 1.0));
    positionCS.xyz /= positionCS.w;
    
    positionCS.xy = positionCS.xy * 0.5 + 0.5;
    positionCS.y = 1 - positionCS.y;
    
    return positionCS;
}

float3 SampleSceneColor(float2 uv)
{
    return sceneColor.Sample(defaultSampler, uv).rgb;
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
    if (gBufferDepth > 0.999)
    {
        return output;
    }
    float3 normalVS = SampleNormalVS(uv);
    
    float3 viewDir = normalize(originVS);
    float3 originWS = mul(InverseView, float4(originVS, 1.0)).xyz;
    float3 reflectDir = normalize(reflect(viewDir, normalVS) + hash33(originWS * 10) * 0.2 * (roughness + 0.1));
    
    uint loops = 45;
    
    output.Fade = 1 - saturate(-dot(viewDir, reflectDir));
    output.Fade = output.Fade * output.Fade;
    
    float3 rayOrigin = originVS + reflectDir * 2.0;
    float step = 0.1f;
    float totalDistance = step;
    float thickness = 1.0f;
    
    for (uint i = 0; i < loops; ++i)
    {
        float4 currentUV = ToScreenSpaceUV(rayOrigin);
        float3 currentGBufferPositionVS = SamplePositionVS(currentUV.xy, gBufferDepth);
        
        float zDelta = (rayOrigin.z - currentGBufferPositionVS.z);
        if (gBufferDepth < 0.9999 && zDelta < 0 && zDelta > -thickness)
        {
            output.Hit = true;
            output.UV = currentUV.xy;
            output.Fade *= 1 - smoothstep(0.6, 1.0, (float) i / (float) (loops - 1));
            break;
        }
        
        if (totalDistance > 1.0f)
            step += 0.03f;
        rayOrigin += reflectDir * step;
        totalDistance += step;

    }
    
    if (output.Hit)
    {
        float originalStep = step * 0.5 * 5;
        
        float2 uv = output.UV;
        
        for (uint j = 0; j < 5; ++j)
        {
            uv = ToScreenSpaceUV(rayOrigin).xy;
            float3 currentGBufferPositionVS = SamplePositionVS(uv, gBufferDepth);
            
            float zDelta = rayOrigin.z - currentGBufferPositionVS.z;
            originalStep *= 0.5;
            
            rayOrigin += reflectDir * (zDelta < -thickness ? -originalStep : originalStep);
        }
        
        output.UV = uv;
    }
    
    return output;
}

bool IsNaN(float x)
{
    return !(x < 0.f || x > 0.f || x == 0.f);
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    float4 surfaceSample = surface.Sample(defaultSampler, uv);
    float metallic, roughness, ao;
    UnpackSurface(surfaceSample, metallic, roughness, ao);
    
    
    TraceOutput traceOutput = Trace(uv, roughness);
    if (traceOutput.Hit)
    {
        float3 sceneColor = SampleSceneColor(traceOutput.UV);
        float fade = traceOutput.Fade;
        float2 centeredUV = traceOutput.UV - 0.5;
        fade *= smoothstep(0.5, 0.499, length(centeredUV));
        
        // prevents nans from previous frames affecting this one
        if (IsNaN(sceneColor.x) || IsNaN(sceneColor.y) || IsNaN(sceneColor.z))
            return 0;
        
        return float4(sceneColor, fade);
    }
    
    return 0;
}