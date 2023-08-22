#pragma once

#include "ResourceId.h"

namespace ResourceIds
{
    class User
    {
    public:
        static inline const RenderGraph::ResourceId ExampleRenderTarget1 = RenderGraph::ResourceIds::GetResourceId(L"ExampleRenderTargetName1");
        static inline const RenderGraph::ResourceId ExampleRenderTarget2 = RenderGraph::ResourceIds::GetResourceId(L"ExampleRenderTargetName2");
    };
}
