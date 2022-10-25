#ifndef SHADOW_PASS_MODEL_HLSLI
#define SHADOW_PASS_MODEL_HLSLI

#include <ShaderLibrary/Common/RootSignature.hlsli>

cbuffer ShadowPassModelCBuffer : register(b0, COMMON_ROOT_SIGNATURE_MODEL_SPACE)
{
    matrix g_Model_Model;
    matrix g_Model_InverseTransposeModel;
    matrix g_Model_ViewProjection;
    float4 g_Model_LightDirectionWs;
    float4 g_Model_Bias; // x - depth bias, y - normal bias (should be negative)
    uint g_Model_LightType; // 0 - directional, 1 - point, 2 - spot
    float3 _g_Model_Padding;
};

#endif
