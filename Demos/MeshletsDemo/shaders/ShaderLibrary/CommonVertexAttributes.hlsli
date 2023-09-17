#ifndef COMMON_VERTEX_ATTRIBUTES_HLSLI
#define COMMON_VERTEX_ATTRIBUTES_HLSLI

struct CommonVertexAttributes
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 uv : TEXCOORD0;
    float4 tangent : TANGENT;
    float4 bitangent : BINORMAL;
};

#endif
