#ifndef COMMON_ROOT_SIGNATURE_HLSLI
#define COMMON_ROOT_SIGNATURE_HLSLI

#define COMMON_ROOT_SIGNATURE_MODEL_SPACE space1
#define COMMON_ROOT_SIGNATURE_PIPELINE_SPACE space2

SamplerState g_Common_PointSampler : register(s0);
SamplerState g_Common_LinearSampler : register(s1);

#endif