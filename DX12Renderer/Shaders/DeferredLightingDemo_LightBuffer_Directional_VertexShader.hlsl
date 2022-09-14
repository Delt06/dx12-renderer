struct VertexShaderInput
{
    float4 PositionOS : POSITION;
    float2 UV : TEXCOORD;
};

struct VertexShaderOutput
{
    float2 UV : TEXCOORD;
    float4 PositionCS : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput IN)
{
    VertexShaderOutput OUT;
    OUT.PositionCS = IN.PositionOS;
    OUT.UV = IN.UV;
    return OUT;
}