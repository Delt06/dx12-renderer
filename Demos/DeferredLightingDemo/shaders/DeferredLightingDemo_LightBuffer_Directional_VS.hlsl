struct VertexShaderInput
{
    float3 PositionOS : POSITION;
};

struct VertexShaderOutput
{
    float4 PositionCS : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput IN)
{
    VertexShaderOutput OUT;
    OUT.PositionCS = float4(IN.PositionOS, 1);
    return OUT;
}