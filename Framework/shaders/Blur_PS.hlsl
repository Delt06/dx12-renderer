struct PixelShaderInput
{
    float2 UV : TEXCOORD;
};

struct BlurParameters
{
    float2 OneOverTexelSize;
    float2 Direction;
};

ConstantBuffer<BlurParameters> blurParametersCB : register(b0);
Texture2D source : register(t0);
SamplerState sourceSampler : register(s0);

static const float GAUSSIAN_OFFSETS[] = 
{ 
    0.0, 1.0, 2.0, 3.0, 4.0 
};

static const float GAUSSIAN_WEIGHTS[] =
{
    0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162
};


float4 main(PixelShaderInput IN) : SV_Target
{
    float2 uv = IN.UV;
    float2 offset = blurParametersCB.OneOverTexelSize;
    
    
    float4 result = source.Sample(sourceSampler, uv) * GAUSSIAN_WEIGHTS[0];

    // https://stackoverflow.com/questions/36303950/hlsl-gaussian-blur-effect
    //(1.0, 0.0) -> horizontal blur
    //(0.0, 1.0) -> vertical blur
    float hstep = blurParametersCB.OneOverTexelSize.x * blurParametersCB.Direction.x;
    float vstep = blurParametersCB.OneOverTexelSize.y * blurParametersCB.Direction.y;

    [unroll]
    for (int i = 1; i < 5; i++)
    {
        result +=
            source.Sample(sourceSampler, uv + float2(hstep * GAUSSIAN_OFFSETS[i], vstep * GAUSSIAN_OFFSETS[i])) * GAUSSIAN_WEIGHTS[i] +
            source.Sample(sourceSampler, uv - float2(hstep * GAUSSIAN_OFFSETS[i], vstep * GAUSSIAN_OFFSETS[i])) * GAUSSIAN_WEIGHTS[i];
    }
    
    return result;
}