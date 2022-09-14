
struct PixelShaderInput
{
    float3 NormalWs : NORMAL;
    float3 TangentWs : TANGENT;
    float3 BitangentWs : BINORMAL;
    float2 Uv : TEXCOORD0;
};

struct PixelShaderOutput
{
    float4 DiffuseColor : SV_TARGET0;
    float4 Normal : SV_TARGET1;
};

Texture2D diffuseMap : register(t0);
Texture2D normalMap : register(t1);
SamplerState defaultSampler : register(s0);

float3 UnpackNormal(const float3 n)
{
    return n * 2.0f - 1.0f;
}

float3 PackNormal(const float3 n)
{
    return (n + 1.0f) * 0.5f;
}

float3 ApplyNormalMap(const float3x3 tbn, Texture2D map, const float2 uv)
{
    const float3 normalTs = UnpackNormal(map.Sample(defaultSampler, uv).xyz);
    return normalize(mul(normalTs, tbn));
}

PixelShaderOutput main(PixelShaderInput IN)
{
    PixelShaderOutput OUT;
    
    float2 uv = IN.Uv;
    
    OUT.DiffuseColor = diffuseMap.Sample(defaultSampler, uv);
    
    // move to WS
    const float3 tangent = normalize(IN.TangentWs);
    const float3 bitangent = normalize(IN.BitangentWs);
    const float3 normal = normalize(IN.NormalWs);

    const float3x3 tbn = float3x3(tangent,
		                              bitangent,
		                              normal);
    OUT.Normal.xyz = PackNormal(ApplyNormalMap(tbn, normalMap, uv));

    
    return OUT;
}
