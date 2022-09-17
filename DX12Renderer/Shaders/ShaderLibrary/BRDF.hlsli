#ifndef BRDF_HLSLI
#define BRDF_HLSLI

#include "Math.hlsli"

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
    float3 Irradiance;
};

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 FresnelSchlick(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
	
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float3 ComputeF0(in BRDFInput input)
{
    float3 F0 = 0.04;
    return lerp(F0, input.DiffuseColor, input.Metallic);
}

float3 ComputeBRDF(in BRDFInput input)
{
    float3 eyeWS = normalize(input.CameraPositionWS - input.PositionWS);
    float3 halfVectorWS = normalize(eyeWS + input.LightDirectionWS);
    
    float3 radiance = input.LightColor;
    
    float3 F0 = ComputeF0(input);
    float3 F = FresnelSchlick(max(dot(halfVectorWS, eyeWS), 0.0), F0);
    
    float NDF = DistributionGGX(input.NormalWS, halfVectorWS, input.Roughness);
    float G = GeometrySmith(input.NormalWS, eyeWS, input.LightDirectionWS, input.Roughness);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(input.NormalWS, eyeWS), 0.0) * max(dot(input.NormalWS, input.LightDirectionWS), 0.0) + 0.0001;
    float3 specular = numerator / denominator;
    
    float3 kS = F;
    float3 kD = 1.0 - kS;
  
    kD *= 1.0 - input.Metallic;
    
    float NdotL = max(dot(input.NormalWS, input.LightDirectionWS), 0.0);
    
    return (kD * input.DiffuseColor / PI + specular) * input.LightColor * NdotL;
}

float3 ComputeBRDFAmbient(in BRDFInput input)
{
    float3 eyeWS = normalize(input.CameraPositionWS - input.PositionWS);
    
    float3 F0 = ComputeF0(input);
    float3 F = FresnelSchlick(max(dot(input.NormalWS, eyeWS), 0.0), F0, input.Roughness);;
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - input.Metallic;
    
    float3 diffuse = input.Irradiance * input.DiffuseColor;
    
    // TODO: implement prefilterMap
    // https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/2.2.1.ibl_specular/2.2.1.pbr.fs
    float3 specular = input.Irradiance * (F * 0.05 + 0.00);
    
    float3 ambient = (kD * diffuse + specular) * input.AmbientOcclusion;
    return ambient;
}

#endif