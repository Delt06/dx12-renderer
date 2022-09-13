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
    float3 Normal : NORMAL;
    float2 Uv : TEXCOORD;
};

struct VertexShaderOutput
{
    float3 NormalWs : NORMAL;
    float2 Uv : TEXCOORD0;
    float4 PositionCs : SV_POSITION;
};

VertexShaderOutput main(VertexAttributes IN)
{
    VertexShaderOutput OUT;

    OUT.NormalWs = mul((float3x3) matricesCb.InverseTransposeModel, IN.Normal);
    OUT.Uv = IN.Uv;
    OUT.PositionCs = mul(matricesCb.ModelViewProjection, float4(IN.PositionOs, 1.0));

    return OUT;
}
