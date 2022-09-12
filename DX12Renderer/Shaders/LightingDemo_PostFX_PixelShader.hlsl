struct PixelShaderInput
{
    float2 Uv : TEXCOORD;
};

struct PostFxParameters
{
    matrix ProjectionInverse;
    float3 FogColor;
    float FogDensity;
};

ConstantBuffer<PostFxParameters> postFxParametersCb : register(b0);

Texture2D sourceColorTexture : register(t0);
Texture2D sourceDepthTexture : register(t1);
SamplerState sourceSampler : register(s0);

// https://www.prkz.de/blog/1-linearizing-depth-buffer-samples-in-hlsl
float GetDepthLinear(float nonLinearDepth)
{
    // We are only interested in the depth here
    const float4 ndcCoords = float4(0, 0, nonLinearDepth, 1.0f);
    // Unproject the vector into (homogenous) view-space vector
    const float4 viewCoords = mul(postFxParametersCb.ProjectionInverse, ndcCoords);
    // Divide by w, which results in actual view-space z value
    const float linearDepth = viewCoords.z / viewCoords.w;
    return linearDepth;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    const float nonLinearDepth = sourceDepthTexture.Sample(sourceSampler, IN.Uv);
    
    float fogFactor;
    if (nonLinearDepth == 1.0)
    {
        fogFactor = 1.0;
    }
    else
    {
        const float depth = GetDepthLinear(nonLinearDepth);
        float exponentArg = depth * postFxParametersCb.FogDensity;
        exponentArg = exponentArg * exponentArg;
        fogFactor = 1 / exp(exponentArg);
    }
    
    const float4 srcColor = sourceColorTexture.Sample(sourceSampler, IN.Uv);
    const float3 resultColor = lerp(postFxParametersCb.FogColor, srcColor.rgb, fogFactor);
    return float4(resultColor, srcColor.a);
}