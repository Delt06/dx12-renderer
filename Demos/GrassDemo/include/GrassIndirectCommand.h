#pragma once

#include <cstdint>

#include <d3d12.h>

struct GrassIndirectCommand
{
    uint32_t Index;
    D3D12_DRAW_INDEXED_ARGUMENTS DrawArguments;
};
