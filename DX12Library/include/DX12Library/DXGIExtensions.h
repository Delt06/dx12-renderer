#pragma once

#include <d3d12.h>
#include <stdint.h>
#include <functional>

bool operator==(const DXGI_SAMPLE_DESC& first, const DXGI_SAMPLE_DESC& second);

bool operator!=(const DXGI_SAMPLE_DESC& first, const DXGI_SAMPLE_DESC& second);

namespace std
{
    template <>
    struct hash<DXGI_SAMPLE_DESC>
    {
        std::size_t operator()(const DXGI_SAMPLE_DESC& desc) const;
    };
};
