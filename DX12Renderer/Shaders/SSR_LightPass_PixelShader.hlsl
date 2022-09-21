struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

Texture2D traceResult : register(t0);
SamplerState traceSampler : register(s0);

cbuffer ParametersCBuffer : register(b0)
{
    float2 TexelSize;
    float Separation;
    int Size;
}

float4 Sample(float2 uv)
{
    return traceResult.Sample(traceSampler, uv);

}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    if (Size <= 0)
    {
        return Sample(uv);
    }
    
    float4 result;
    
    for (int i = -Size; i <= Size; ++i)
    {
        for (int j = -Size; j <= Size; ++j)
        {
            result += Sample(uv + float2(i, j) * Separation * TexelSize);
        }
    }
    
    int kernelSide = Size * 2 + 1;
    result /= kernelSide * kernelSide;
    return result * result.a;
}