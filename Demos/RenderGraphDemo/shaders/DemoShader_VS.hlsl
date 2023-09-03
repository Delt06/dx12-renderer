struct VertexAttributes
{
    float3 PositionOS : POSITION;
};

struct Varyings
{
    float4 PositionCS : SV_POSITION;
};

Varyings main(VertexAttributes IN)
{
    Varyings OUT;

    OUT.PositionCS = float4(IN.PositionOS, 1);

    return OUT;
}
