TextureCube skybox : register(t0);
SamplerState skyboxSampler : register(s0);

struct PixelShaderInput
{
    float3 CubemapUV : CUBEMAP_UV;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    return skybox.Sample(skyboxSampler, IN.CubemapUV);
}