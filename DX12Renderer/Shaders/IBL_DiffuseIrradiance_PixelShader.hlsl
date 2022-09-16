#include <ShaderLibrary/Math.hlsli>

struct PixelShaderInput
{
    float3 Normal : TEXCOORD;
};

TextureCube source : register(t0);
SamplerState sourceSampler : register(s0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    // https://learnopengl.com/PBR/IBL/Diffuse-irradiance
    float3 irradiance = 0.0;
    
    float3 normal = normalize(IN.Normal);
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += source.Sample(sourceSampler, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    
    irradiance = PI * irradiance * (1.0 / nrSamples);
    return float4(irradiance, 1);
}