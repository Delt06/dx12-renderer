#pragma once

#include <memory>

#include <Framework/CommonRootSignature.h>

#include <RenderGraph/RenderGraphRoot.h>

namespace RenderGraph
{
    class User
    {
    public:
        static std::unique_ptr<RenderGraphRoot> Create(
            const std::shared_ptr<CommonRootSignature>& rootSignature,
            CommandList& commandList
        );
    };
}
