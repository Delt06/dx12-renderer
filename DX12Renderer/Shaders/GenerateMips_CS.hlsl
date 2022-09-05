/**
 * Compute shader to generate mipmaps for a given texture.
 * Source: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli
 */

// size of a thread group along a single dimension.
#define BLOCK_SIZE 8

#define WIDTH_HEIGHT_EVEN 0     // Both the width and the height of the texture are even.
#define WIDTH_ODD_HEIGHT_EVEN 1 // The texture width is odd and the height is even.
#define WIDTH_EVEN_HEIGHT_ODD 2 // The texture width is even and the height is odd.
#define WIDTH_HEIGHT_ODD 3      // Both the width and height of the texture are odd.

struct ComputeShaderInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadId;
	uint GroupIndex : SV_GroupIndex;
};

cbuffer GenerateMipsCb : register(b0)
{
uint srcMipLevel;
uint numMipLevels; // Number of OutMips to write: [1-4]
uint srcDimensionMask; // Width and height of the source texture are even or odd.
bool isSRgb; // Must apply gamma correction to sRGB textures.
float2 texelSize; // 1.0 / OutMip1.Dimensions
}

// Source mip map
Texture2D<float4> srcMip : register(t0);

// Write up to 4 mip map levels.
RWTexture2D<float4> outMip1 : register(u0);
RWTexture2D<float4> outMip2 : register(u1);
RWTexture2D<float4> outMip3 : register(u2);
RWTexture2D<float4> outMip4 : register(u3);

// Linear clamp sampler
SamplerState linearClampSampler : register(s0);

#define GenerateMips_RootSignature \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 6), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1) )," \
    "DescriptorTable( UAV(u0, numDescriptors = 4) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

// The reason for separating channels is to reduce bank conflicts in the
// local data memory controller.  A large stride will cause more threads
// to collide on the same memory bank.
groupshared float gsR[BLOCK_SIZE * BLOCK_SIZE];
groupshared float gsG[BLOCK_SIZE * BLOCK_SIZE];
groupshared float gsB[BLOCK_SIZE * BLOCK_SIZE];
groupshared float gsA[BLOCK_SIZE * BLOCK_SIZE];

void StoreColor(uint index, float4 color)
{
	gsR[index] = color.r;
	gsG[index] = color.g;
	gsB[index] = color.b;
	gsA[index] = color.a;
}

float4 LoadColor(uint index)
{
	return float4(gsR[index], gsG[index], gsB[index], gsA[index]);
}

// Source: https://en.wikipedia.org/wiki/SRGB#The_reverse_transformation
float3 SRgbToLinear(const float3 x)
{
	return x < 0.04045f ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

// Source: https://en.wikipedia.org/wiki/SRGB#The_forward_transformation_(CIE_XYZ_to_sRGB)
float3 ConvertToSRgb(const float3 x)
{
	return x < 0.0031308 ? 12.92 * x : 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;
}

// Convert linear color to sRGB before storing if the original source is 
// an sRGB texture.
float4 PackColor(float4 x)
{
	if (isSRgb)
	{
		return float4(ConvertToSRgb(x.rgb), x.a);
	}

	return x;
}


[RootSignature(GenerateMips_RootSignature)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
	float4 src1 = 0;

	// One bilinear sample is insufficient when scaling down by more than 2x.
	// You will slightly undersample in the case where the source dimension
	// is odd.  This is why it's a really good idea to only generate mips on
	// power-of-two sized textures.  Trying to handle the undersampling case
	// will force this shader to be slower and more complicated as it will
	// have to take more source texture samples.

	// Determine the path to use based on the dimension of the 
	// source texture.
	// 0b00(0): Both width and height are even.
	// 0b01(1): Width is odd, height is even.
	// 0b10(2): Width is even, height is odd.
	// 0b11(3): Both width and height are odd.

	switch (srcDimensionMask)
	{
	case WIDTH_HEIGHT_EVEN:
		{
			const float2 uv = texelSize * (IN.DispatchThreadId.xy + 0.5);
			src1 = srcMip.SampleLevel(linearClampSampler, uv, srcMipLevel);
		}
		break;
	case WIDTH_ODD_HEIGHT_EVEN:
		{
			// > 2:1 in X dimension
			// Use 2 bilinear samples to guarantee we don't undersample when downsizing by more than 2x
			// horizontally.
			const float2 uv = texelSize * (IN.DispatchThreadId.xy + float2(0.25f, 0.5f));
			const float offset = texelSize * float2(0.5f, 0.0f);
			src1 = 0.5f * (srcMip.SampleLevel(linearClampSampler, uv, srcMipLevel) + srcMip.SampleLevel(
				linearClampSampler, uv + offset, srcMipLevel));
		}
	case WIDTH_EVEN_HEIGHT_ODD:
		{
			// > 2:1 in Y dimension
			// Use 2 bilinear samples to guarantee we don't undersample when downsizing by more than 2x
			// vertically.
			const float2 uv = texelSize * (IN.DispatchThreadId.xy + float2(0.5f, 0.25f));
			const float offset = texelSize * float2(0.0f, 0.5f);
			src1 = 0.5f * (srcMip.SampleLevel(linearClampSampler, uv, srcMipLevel) + srcMip.SampleLevel(
				linearClampSampler, uv + offset, srcMipLevel));
		}

	case WIDTH_HEIGHT_ODD:
		{
			// > 2:1 in in both dimensions
			// Use 4 bilinear samples to guarantee we don't undersample when downsizing by more than 2x
			// in both directions.
			const float2 uv = texelSize * (IN.DispatchThreadId.xy + float2(0.25f, 0.25f));
			const float offset = texelSize * 0.5f;

			src1 = 0.25f * (
				srcMip.SampleLevel(linearClampSampler, uv, srcMipLevel) +
				srcMip.SampleLevel(linearClampSampler, uv + float2(offset.x, 0.0f), srcMipLevel) +
				srcMip.SampleLevel(linearClampSampler, uv + float2(0.0f, offset.y), srcMipLevel) +
				srcMip.SampleLevel(linearClampSampler, uv + float2(offset.x, offset.y), srcMipLevel)
			);
		}
		break;
	}

	outMip1[IN.DispatchThreadId.xy] = PackColor(src1);

	if (numMipLevels == 1)
		return;

	// Without lane swizzle operations, the only way to share data with other
	// threads is through LDS.
	StoreColor(IN.GroupIndex, src1);

	// This guarantees all LDS writes are complete and that all threads have
	// executed all instructions so far (and therefore have issued their LDS
	// write instructions.)
	GroupMemoryBarrierWithGroupSync();

	// With low three bits for X and high three bits for Y, this bit mask
	// (binary: 001001) checks that X and Y are even.
	if ((IN.GroupIndex & 0x9) == 0)
	{
		const float4 src2 = LoadColor(IN.GroupIndex + 0x01);
		const float4 src3 = LoadColor(IN.GroupIndex + 0x08);
		const float4 src4 = LoadColor(IN.GroupIndex + 0x09);
		src1 = 0.25 * (src1 + src2 + src3 + src4);

		outMip2[IN.DispatchThreadId.xy / 2] = PackColor(src1);
		StoreColor(IN.GroupIndex, src1);
	}

	if (numMipLevels == 2)
		return;

	GroupMemoryBarrierWithGroupSync();

	// This bit mask (binary: 011011) checks that X and Y are multiples of four.
	if ((IN.GroupIndex & 0x1B) == 0)
	{
		const float4 src2 = LoadColor(IN.GroupIndex + 0x02);
		const float4 src3 = LoadColor(IN.GroupIndex + 0x10);
		const float4 src4 = LoadColor(IN.GroupIndex + 0x12);
		src1 = 0.25 * (src1 + src2 + src3 + src4);

		outMip3[IN.DispatchThreadId.xy / 4] = PackColor(src1);
		StoreColor(IN.GroupIndex, src1);
	}

	if (numMipLevels == 3)
		return;

	GroupMemoryBarrierWithGroupSync();

	// This bit mask would be 111111 (X & Y multiples of 8), but only one
	// thread fits that criteria.
	if (IN.GroupIndex == 0)
	{
		const float4 src2 = LoadColor(IN.GroupIndex + 0x04);
		const float4 src3 = LoadColor(IN.GroupIndex + 0x20);
		const float4 src4 = LoadColor(IN.GroupIndex + 0x24);
		src1 = 0.25 * (src1 + src2 + src3 + src4);

		outMip4[IN.DispatchThreadId.xy / 8] = PackColor(src1);
	}
}
