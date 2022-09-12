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
float GetDepthLinear(float2 uv)
{
    float nonLinearDepth = sourceDepthTexture.Sample(sourceSampler, uv);
    // We are only interested in the depth here
    float4 ndcCoords = float4(0, 0, nonLinearDepth, 1.0f);
    // Unproject the vector into (homogenous) view-space vector
    float4 viewCoords = mul(postFxParametersCb.ProjectionInverse, ndcCoords);
    // Divide by w, which results in actual view-space z value
    float linearDepth = viewCoords.z / viewCoords.w;
    return linearDepth;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    const float depth = GetDepthLinear(IN.Uv);
    float exponentArg = depth * postFxParametersCb.FogDensity;
    exponentArg = exponentArg * exponentArg;
    const float fogFactor = 1 / exp(exponentArg);
    const float4 srcColor = sourceColorTexture.Sample(sourceSampler, IN.Uv);
    const float3 resultColor = lerp(postFxParametersCb.FogColor, srcColor.rgb, fogFactor);
    return float4(resultColor, srcColor.a);
}