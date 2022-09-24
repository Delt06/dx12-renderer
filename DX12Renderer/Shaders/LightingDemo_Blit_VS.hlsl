struct VertexShaderInput
{
    float3 PositionOs : POSITION;
    float3 Normal : NORMAL;
    float2 Uv : TEXCOORD;
};

struct VertexShaderOutput
{
    float2 Uv : TEXCOORD;
    float4 PositionCs : SV_Position;
};

VertexShaderOutput main(VertexShaderInput IN)
{
    VertexShaderOutput OUT;
    OUT.PositionCs = float4(IN.PositionOs, 1.0);
    OUT.Uv = IN.Uv;
    return OUT;
}