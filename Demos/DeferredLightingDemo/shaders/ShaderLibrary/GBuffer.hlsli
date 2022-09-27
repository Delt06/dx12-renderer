#ifndef GBUFFER_HLSLI
#define GBUFFER_HLSLI

Texture2D gBufferDiffuse : register(t0);
Texture2D gBufferNormalsWS : register(t1);
Texture2D gBufferSurface : register(t2);
Texture2D gBufferDepth : register(t3);

SamplerState gBufferSampler : register(s0);

#endif