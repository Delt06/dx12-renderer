#pragma once

#include <cstdint>

namespace RenderGraph
{
    struct RenderMetadata
    {
        uint32_t m_ScreenWidth;
        uint32_t m_ScreenHeight;
        double m_Time;
        uint64_t m_FrameIndex;
    };
}

