#pragma once

#define WIN32_LEAN_AND_MEAN
#include <exception>
#include <string>
#include <Windows.h> // For HRESULT
#include <pix3.h>

#include <d3d12.h>

// From DXSampleHelper.h
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(const HRESULT result)
{
	if (FAILED(result))
	{
		const std::string message = std::to_string(result);
		throw std::exception(message.c_str());
	}
}

inline void ThrowIfNegative(const int value)
{
	if (value < 0)
	{
		throw std::exception();
	}
}

inline void Assert(const bool condition, const char* message)
{
	if (!condition)
	{
		throw std::exception(message);
	}
}

#define PIXScope(commandList, eventName) PIXScopedEvent((commandList).GetGraphicsCommandList().Get(), PIX_COLOR_DEFAULT, eventName);
#define PIXScopeCPU(eventName) PIXScopedEvent(PIX_COLOR_DEFAULT, eventName);

// Hashers for view descriptions.
namespace std
{
	// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
	template <typename T>
	void hash_combine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	template <>
	struct hash<D3D12_SHADER_RESOURCE_VIEW_DESC>
	{
		std::size_t operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, srvDesc.Format);
			hash_combine(seed, srvDesc.ViewDimension);
			hash_combine(seed, srvDesc.Shader4ComponentMapping);

			switch (srvDesc.ViewDimension)
			{
			case D3D12_SRV_DIMENSION_BUFFER:
				hash_combine(seed, srvDesc.Buffer.FirstElement);
				hash_combine(seed, srvDesc.Buffer.NumElements);
				hash_combine(seed, srvDesc.Buffer.StructureByteStride);
				hash_combine(seed, srvDesc.Buffer.Flags);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE1D:
				hash_combine(seed, srvDesc.Texture1D.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture1D.MipLevels);
				hash_combine(seed, srvDesc.Texture1D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, srvDesc.Texture1DArray.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture1DArray.MipLevels);
				hash_combine(seed, srvDesc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, srvDesc.Texture1DArray.ArraySize);
				hash_combine(seed, srvDesc.Texture1DArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2D:
				hash_combine(seed, srvDesc.Texture2D.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture2D.MipLevels);
				hash_combine(seed, srvDesc.Texture2D.PlaneSlice);
				hash_combine(seed, srvDesc.Texture2D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, srvDesc.Texture2DArray.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture2DArray.MipLevels);
				hash_combine(seed, srvDesc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, srvDesc.Texture2DArray.ArraySize);
				hash_combine(seed, srvDesc.Texture2DArray.PlaneSlice);
				hash_combine(seed, srvDesc.Texture2DArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMS:
				//                hash_combine(seed, srvDesc.Texture2DMS.UnusedField_NothingToDefine);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
				hash_combine(seed, srvDesc.Texture2DMSArray.FirstArraySlice);
				hash_combine(seed, srvDesc.Texture2DMSArray.ArraySize);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE3D:
				hash_combine(seed, srvDesc.Texture3D.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture3D.MipLevels);
				hash_combine(seed, srvDesc.Texture3D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURECUBE:
				hash_combine(seed, srvDesc.TextureCube.MostDetailedMip);
				hash_combine(seed, srvDesc.TextureCube.MipLevels);
				hash_combine(seed, srvDesc.TextureCube.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
				hash_combine(seed, srvDesc.TextureCubeArray.MostDetailedMip);
				hash_combine(seed, srvDesc.TextureCubeArray.MipLevels);
				hash_combine(seed, srvDesc.TextureCubeArray.First2DArrayFace);
				hash_combine(seed, srvDesc.TextureCubeArray.NumCubes);
				hash_combine(seed, srvDesc.TextureCubeArray.ResourceMinLODClamp);
				break;
				// TODO: Update Visual Studio?
				//case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
				//    hash_combine(seed, srvDesc.RaytracingAccelerationStructure.Location);
				//    break;
			}

			return seed;
		}
	};

	template <>
	struct hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>
	{
		std::size_t operator()(const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, uavDesc.Format);
			hash_combine(seed, uavDesc.ViewDimension);

			switch (uavDesc.ViewDimension)
			{
			case D3D12_UAV_DIMENSION_BUFFER:
				hash_combine(seed, uavDesc.Buffer.FirstElement);
				hash_combine(seed, uavDesc.Buffer.NumElements);
				hash_combine(seed, uavDesc.Buffer.StructureByteStride);
				hash_combine(seed, uavDesc.Buffer.CounterOffsetInBytes);
				hash_combine(seed, uavDesc.Buffer.Flags);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE1D:
				hash_combine(seed, uavDesc.Texture1D.MipSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, uavDesc.Texture1DArray.MipSlice);
				hash_combine(seed, uavDesc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, uavDesc.Texture1DArray.ArraySize);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE2D:
				hash_combine(seed, uavDesc.Texture2D.MipSlice);
				hash_combine(seed, uavDesc.Texture2D.PlaneSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, uavDesc.Texture2DArray.MipSlice);
				hash_combine(seed, uavDesc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, uavDesc.Texture2DArray.ArraySize);
				hash_combine(seed, uavDesc.Texture2DArray.PlaneSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE3D:
				hash_combine(seed, uavDesc.Texture3D.MipSlice);
				hash_combine(seed, uavDesc.Texture3D.FirstWSlice);
				hash_combine(seed, uavDesc.Texture3D.WSize);
				break;
			}

			return seed;
		}
	};
}


namespace Math
{
	constexpr float PI = 3.1415926535897932384626433832795f;
	constexpr float _2PI = 2.0f * PI;
	// Convert radians to degrees.
	constexpr float Degrees(const float radians)
	{
		return radians * (180.0f / PI);
	}

	// Convert degrees to radians.
	constexpr float Radians(const float degrees)
	{
		return degrees * (PI / 180.0f);
	}

	template <typename T>
	T Deadzone(T val, T deadzone)
	{
		if (std::abs(val) < deadzone)
		{
			return T(0);
		}

		return val;
	}

	// Normalize a value in the range [min - max]
	template <typename T, typename U>
	T NormalizeRange(U x, U min, U max)
	{
		return T(x - min) / T(max - min);
	}

	// Shift and bias a value into another range.
	template <typename T, typename U>
	T ShiftBias(U x, U shift, U bias)
	{
		return T(x * bias) + T(shift);
	}

	/***************************************************************************
	* These functions were taken from the MiniEngine.
	* Source code available here:
	* https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Math/Common.h
	* Retrieved: January 13, 2016
	**************************************************************************/
	template <typename T>
	T AlignUpWithMask(T value, const size_t mask)
	{
		return static_cast<T>((static_cast<size_t>(value) + mask) & ~mask);
	}

	template <typename T>
	T AlignDownWithMask(T value, const size_t mask)
	{
		return static_cast<T>(static_cast<size_t>(value) & ~mask);
	}

	template <typename T>
	T AlignUp(T value, const size_t alignment)
	{
		return AlignUpWithMask(value, alignment - 1);
	}

	template <typename T>
	T AlignDown(T value, const size_t alignment)
	{
		return AlignDownWithMask(value, alignment - 1);
	}

	template <typename T>
	bool IsAligned(T value, const size_t alignment)
	{
		return 0 == (static_cast<size_t>(value) & (alignment - 1));
	}

	template <typename T>
	T DivideByMultiple(T value, size_t alignment)
	{
		return static_cast<T>((value + alignment - 1) / alignment);
	}

	/***************************************************************************/

	/**
	* Round up to the next highest power of 2.
	* @source: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	* @retrieved: January 16, 2016
	*/
	inline uint32_t NextHighestPow2(uint32_t v)
	{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;

		return v;
	}

	/**
	* Round up to the next highest power of 2.
	* @source: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	* @retrieved: January 16, 2016
	*/
	inline uint64_t NextHighestPow2(uint64_t v)
	{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v |= v >> 32;
		v++;

		return v;
	}
}

#define STR1(x) #x
#define STR(x) STR1(x)
#define WSTR1(x) L##x
#define WSTR(x) WSTR1(x)
#define NAME_D3D12_OBJECT(x) x->SetName( WSTR(__FILE__ "(" STR(__LINE__) "): " L#x) )
