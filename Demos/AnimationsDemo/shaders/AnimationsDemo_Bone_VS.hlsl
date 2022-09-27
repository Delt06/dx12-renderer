struct Matrices
{
    matrix Model;
    matrix ModelView;
    matrix InverseTransposeModelView;
    matrix ModelViewProjection;
    matrix View;
    matrix Projection;
    matrix InverseTransposeModel;
    float4 CameraPosition;
};

ConstantBuffer<Matrices> matricesCb : register(b0);

struct VertexAttributes
{
    float3 PositionOs : POSITION;
};

struct VertexShaderOutput
{
    float4 PositionCs : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
    VertexShaderOutput OUT;
    
    OUT.PositionCs = mul(matricesCb.ModelViewProjection, float4(IN.PositionOs, 1.0));

    return OUT;
}
