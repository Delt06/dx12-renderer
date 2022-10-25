#ifndef MODEL_HLSLI
#define MODEL_HLSLI

#include <ShaderLibrary/Common/RootSignature.hlsli>

cbuffer ModelCBuffer : register(b0, COMMON_ROOT_SIGNATURE_MODEL_SPACE)
{
    matrix g_Model_Model;
    matrix g_Model_ModelViewProjection;
    matrix g_Model_InverseTransposeModel;

    matrix g_Model_Taa_PreviousModelViewProjectionMatrix;
};

#endif