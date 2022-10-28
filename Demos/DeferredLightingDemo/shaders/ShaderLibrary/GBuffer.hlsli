#ifndef GBUFFER_HLSLI
#define GBUFFER_HLSLI

#include <ShaderLibrary/Common/RootSignature.hlsli>

Texture2D gBufferDiffuse : register(t0, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
Texture2D gBufferNormalsWS : register(t1, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
Texture2D gBufferSurface : register(t2, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);
Texture2D<float> gBufferDepth : register(t3, COMMON_ROOT_SIGNATURE_PIPELINE_SPACE);

SamplerState gBufferSampler : register(s2);

#endif
