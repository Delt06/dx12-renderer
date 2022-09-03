#pragma once

#define WIN32_LEAN_AND_MEAN
#include <exception>
#include <Windows.h> // For HRESULT

// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(const HRESULT result)
{
	if (FAILED(result))
	{
		throw std::exception();
	}
}

inline void ThrowIfNegative(const int value)
{
	if (value < 0)
	{
		throw std::exception();
	}
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
