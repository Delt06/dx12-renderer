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