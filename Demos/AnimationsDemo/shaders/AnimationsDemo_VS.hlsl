#include "ShaderLibrary/Model.hlsli"

struct Bone
{
    matrix Transform;
};

StructuredBuffer<Bone> bonesSb : register(t0);

struct VertexAttributes
{
    float3 PositionOs : POSITION;
    float3 Normal : NORMAL;
    float2 Uv : TEXCOORD;
    uint4 BoneIds : BONE_IDS;
    float4 BoneWeights : BONE_WEIGHTS;
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
    
    float4 positionOs = float4(IN.PositionOs, 1.0f);
    
    // skinning
    {
        matrix boneTransform = bonesSb[IN.BoneIds[0]].Transform * IN.BoneWeights[0];
        boneTransform += bonesSb[IN.BoneIds[1]].Transform * IN.BoneWeights[1];
        boneTransform += bonesSb[IN.BoneIds[2]].Transform * IN.BoneWeights[2];
        boneTransform += bonesSb[IN.BoneIds[3]].Transform * IN.BoneWeights[3];
        
        positionOs = mul(boneTransform, positionOs);
    }

    OUT.NormalWs = mul((float3x3) g_Model_InverseTransposeModel, IN.Normal);
    OUT.Uv = IN.Uv;
    OUT.PositionCs = mul(g_Model_ModelViewProjection, positionOs);

    return OUT;
}
