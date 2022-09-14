#ifndef BRDF_HLSLI
#define BRDF_HLSLI

struct BRDFInput
{
    float3 NormalWS;
    float3 LightDirectionWS;
    float3 LightColor;
    float3 PositionWS;
    float3 CameraPositionWS;
};

float Diffuse(const float3 n, const float3 l)
{
    return max(0, dot(n, l));
}

float Specular(const float3 v, const float3 n, const float3 l, const float specularPower)
{
    const float3 r = normalize(reflect(-l, n));
    const float rDotV = max(0, dot(r, v));

    return pow(rDotV, specularPower);
}

// TODO: change to Cook-Torrance, currently it is Phong
float3 ComputeBRDF(in BRDFInput input)
{
    float3 invEyeWS = normalize(input.CameraPositionWS - input.PositionWS);
    return (Diffuse(input.NormalWS, input.LightDirectionWS) + Specular(invEyeWS, input.NormalWS, input.LightDirectionWS, 10.0)) * input.LightColor;
}

#endif