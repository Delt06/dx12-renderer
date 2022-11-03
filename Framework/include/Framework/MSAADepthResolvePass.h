#pragma once

#include <Framework/ComputeShader.h>
#include <Framework/ShaderResourceView.h>
#include <Framework/UnorderedAccessView.h>

class MSAADepthResolvePass
{
public:
    explicit MSAADepthResolvePass(const std::shared_ptr<CommonRootSignature>& rootSignature);

    void Resolve(CommandList& commandList, const std::shared_ptr<Texture>& source, const std::shared_ptr<Texture>& destination) const;

private:
    std::shared_ptr<CommonRootSignature> m_RootSignature;
    ComputeShader m_ComputeShader;
};
