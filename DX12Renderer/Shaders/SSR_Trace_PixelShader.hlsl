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
    
    float MaxDistance;
    float Resolution;
    uint Steps;
    float Thickness;
    
    float2 TexelSize;
    float2 _Padding;
};

Texture2D sceneColor : register(t0);
Texture2D normals : register(t1);
Texture2D depth : register(t2);

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

float4 SampleSceneColor(float2 uv)
{
    return sceneColor.Sample(defaultSampler, uv);
}

struct TraceOutput
{
    bool Hit;
    float2 UV;
};

#define SAMPLES_COUNT 16

static const float2 RANDOM_PIXEL_OFFSETS[SAMPLES_COUNT] =
{
    float2(0.94558609, -0.76890725),
	float2(-0.81544232, -0.87912464),
	float2(0.97484398, 0.75648379),
	float2(-0.91588581, 0.45771432),
	float2(-0.94201624, -0.39906216),
	float2(-0.094184101, -0.92938870),
	float2(0.34495938, 0.29387760),
	float2(-0.38277543, 0.27676845),
	float2(0.44323325, -0.97511554),
	float2(0.53742981, -0.47373420),
	float2(-0.26496911, -0.41893023),
	float2(0.79197514, 0.19090188),
	float2(-0.24188840, 0.99706507),
	float2(-0.81409955, 0.91437590),
	float2(0.19984126, 0.78641367),
	float2(0.14383161, -0.14100790)
};

TraceOutput Trace(float2 uv)
{
    float3 originVS = SamplePositionVS(uv);
    float initialDepth = originVS.z;
    float3 normalVS = SampleNormalVS(uv);
    
    float3 viewDir = normalize(originVS);
    float3 reflectDir = normalize(reflect(viewDir, normalVS));
    
    float currentLength = 1;
    uint loops = 10;
    
    TraceOutput output = (TraceOutput) 0;
    
    for (uint i = 0; i < loops && !output.Hit; ++i)
    {
        float3 currentPosition = originVS + reflectDir * currentLength;
        float4 currentUV = ToScreenSpaceUV(currentPosition);
        
        float gBufferDepth;
        float3 currentGBufferPositionVS = SamplePositionVS(currentUV.xy, gBufferDepth);
        float currentDepth = currentGBufferPositionVS.z;
        
        for (uint j = 0; j < SAMPLES_COUNT; ++j)
        {
            if (abs(currentPosition.z - currentDepth) < 0.001)
            {
                output.Hit = true;
                output.UV = currentUV.xy;
                break;
            }
            
            // jitter sample depth
            currentDepth = SamplePositionVS(currentUV.xy + RANDOM_PIXEL_OFFSETS[j] * 2 * TexelSize).z;
        }
        
        // hit skybox
        if (gBufferDepth > 0.9999)
        {
            break;
        }
        
        float3 newPosition = currentGBufferPositionVS;
        newPosition.z = currentDepth;
        currentLength = length(originVS - newPosition);
    }
    
    return output;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    TraceOutput traceOutput = Trace(uv);
    if (traceOutput.Hit)
    {
        if (any(traceOutput.UV < 0 || traceOutput.UV > 1))
            return 0;
        return SampleSceneColor(traceOutput.UV);
    }
    
    return 0;
}