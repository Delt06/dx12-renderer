#ifndef GBUFFER_HLSLI
#define GBUFFER_HLSLI

Texture2D gBufferDiffuse : register(t0);
Texture2D gBufferNormalsWS : register(t1);

SamplerState gBufferSampler : register(s0);

#endif