#ifndef BRDF_HLSLI
#define BRDF_HLSLI

struct BRDFInput
{
    float3 NormalWS;
    float3 LightDirectionWS;
    float3 LightColor;
    float3 PositionWS;
    float3 CameraPositionWS;
    float3 DiffuseColor;
    float Metallic;
    float Roughness;
    float AmbientOcclusion;
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
    float3 diffuse = Diffuse(input.NormalWS, input.LightDirectionWS) * input.DiffuseColor;
    float3 specular = Specular(invEyeWS, input.NormalWS, input.LightDirectionWS, 100.0);
    return (diffuse + specular) * input.LightColor;
}

#endif