#ifndef COMMON_ROOT_SIGNATURE_HLSLI
#define COMMON_ROOT_SIGNATURE_HLSLI

#define COMMON_ROOT_SIGNATURE_MODEL_SPACE space1
#define COMMON_ROOT_SIGNATURE_PIPELINE_SPACE space2

SamplerState g_Common_PointWrapSampler : register(s0);
SamplerState g_Common_LinearWrapSampler : register(s1);
SamplerState g_Common_PointClampSampler : register(s2);
SamplerState g_Common_LinearClampSampler : register(s3);

#endif