struct Parameters
{
    float4 Forward;
    float4 Up;
    
    float Roughness;
    float2 SourceResolution;
    float _Padding;
};

ConstantBuffer<Parameters> parametersCB : register(b0);