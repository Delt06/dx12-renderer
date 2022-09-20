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

float3 SamplePositionVS(float2 uv)
{
    float zNDC = depth.SampleLevel(defaultSampler, uv, 0).x;
    float3 positionNDC = ScreenSpaceUVToNDC(uv, zNDC);
    float3 positionVS = RestorePositionVS(positionNDC, InverseProjection).xyz;
    return positionVS;
}

float3 SampleNormalVS(float2 uv)
{
    float3 normalWS = UnpackNormal(normals.Sample(defaultSampler, uv).xyz);
    return normalize(mul((float3x3) View, normalWS));
}

float4 ToScreenSpace(float3 positionVS)
{
    float4 positionCS = mul(Projection, float4(positionVS, 1.0));
    positionCS.xyz /= positionCS.w;
    
    positionCS.xy = positionCS.xy * 0.5 + 0.5;
    positionCS.y = 1 - positionVS.y;
    positionCS.xy /= TexelSize;
    
    return positionCS;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    
    float3 positionFrom = SamplePositionVS(uv);
    float3 directionFrom = normalize(positionFrom);
    float3 normal = SampleNormalVS(uv);
    float3 reflectionDirection = reflect(directionFrom, normal);
    float3 positionTo = positionFrom;
    
    float3 startVS = positionFrom;
    float3 endVS = positionFrom + reflectionDirection * MaxDistance;
    
    // SS is for Screen Space
    float4 startSS = ToScreenSpace(startVS);
    float4 endSS = ToScreenSpace(endVS);
    
    
    float2 deltaVector = endSS.xy - startSS.xy;
    float2 absDeltaVector = abs(deltaVector);
    float useX = absDeltaVector.x >= absDeltaVector.y ? 1 : 0;
    float delta = lerp(absDeltaVector.y, absDeltaVector.x, useX) * saturate(Resolution);
    
    float2 increment = deltaVector / max(delta, 0.001);
    
    // last position where the ray missed/did not intersect any geometry
    float search0 = 0;
    // 0 = start fragment, 1 - end fragment
    float search1 = 0;
    
    // hit during first pass
    uint hit0 = 0;
    // hit during second pass
    uint hit1 = 0;
    
    float viewDistance = startVS.z;
    // view distance different between the current ray point and scene position
    // how far behind/in front of the scene the ray is
    float depth = Thickness;
    
    float2 currentSS = startSS.xy;
    uv.xy = currentSS * TexelSize;
    
    uint i;
    [loop]
    for (i = 0; i < uint(delta); ++i)
    {
        currentSS += increment;
        uv = currentSS * TexelSize;
        positionTo = SamplePositionVS(uv);
        
        float2 offset = (currentSS - startSS.xy) / deltaVector;
        search1 = lerp(offset.y, offset.x, useX);
        search1 = saturate(search1);
        
        viewDistance = (startVS.z * endVS.z) / lerp(endVS.z, startVS.z, search1);
        depth = viewDistance - positionTo.z;
        
        if (0 < depth && depth < Thickness)
        {
            hit0 = 1;
            break;
        }
        else
        {
            search0 = search1;
        }
    }
    
    search1 = (search0 + search1) * 0.5;
    uint steps = Steps * hit0;
    
    [loop]
    for (i = 0; i < steps; ++i)
    {
        currentSS = lerp(startSS.xy, endSS.xy, search1);
        uv = currentSS * TexelSize;
        positionTo = SamplePositionVS(uv);

        viewDistance = (startVS.z * endVS.z) / lerp(endVS.z, startVS.z, search1);
        depth = viewDistance - positionTo.z;
        
        if (0 < depth && depth < Thickness)
        {
            hit1 = 1;
            search1 = (search0 + search1) * 0.5;
        }
        else
        {
            float temp = search1;
            search1 = search1 + ((search1 - search0) / 2);
            search0 = temp;
        }
    }
    
    float visibility = hit1
        //* (1 - max(dot(-directionFrom, reflectionDirection), 0)) // fade out if pointing towards camera
        //* (1 - saturate(depth / Thickness)) // fade out if intersection is too far from scene geometry
        //* (1 - saturate(length(positionTo - positionFrom) / MaxDistance)) // fade out if reflected point is too far
        //* (any(uv < 0 || uv > 1) ? 0 : 1) // fade out if outside of viewport
    ;
    visibility = saturate(visibility);
    
    return sceneColor.Sample(defaultSampler, uv) * visibility;
}