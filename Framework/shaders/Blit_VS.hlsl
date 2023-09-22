struct VertexShaderInput
{
    float3 PositionOS : POSITION;
    float2 UV : TEXCOORD;
};

struct VertexShaderOutput
{
    float2 UV : TEXCOORD;
    float4 PositionCS : SV_Position;
};

VertexShaderOutput main(VertexShaderInput IN)
{
    VertexShaderOutput OUT;
    OUT.PositionCS = float4(IN.PositionOS, 1.0);
    OUT.UV = IN.UV;
    return OUT;
}
