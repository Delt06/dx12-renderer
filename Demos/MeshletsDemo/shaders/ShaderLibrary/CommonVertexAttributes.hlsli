#ifndef COMMON_VERTEX_ATTRIBUTES_HLSLI
#define COMMON_VERTEX_ATTRIBUTES_HLSLI

struct CommonVertexAttributes
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
};

#endif
