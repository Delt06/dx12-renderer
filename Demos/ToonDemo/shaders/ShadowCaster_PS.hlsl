struct VertexShaderInput
{
    float Depth : VIEW_Z;
};

struct PixelShaderOutput
{
    float2 Depth_DepthSqr : SV_TARGET;
};

PixelShaderOutput main(VertexShaderInput IN)
{
    PixelShaderOutput OUT;
    OUT.Depth_DepthSqr = float2(IN.Depth, IN.Depth * IN.Depth);
    return OUT;
}
