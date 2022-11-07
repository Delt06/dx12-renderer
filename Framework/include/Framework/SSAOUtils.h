#pragma once

#include <vector>
#include <random>
#include <memory.h>

#include <DirectXMath.h>

#include <DX12Library/CommandList.h>

class SSAOUtils
{
public:
    static void GenerateRandomSamples(size_t samplesCount, std::vector<DirectX::XMFLOAT4>& samplesDest);

    static std::shared_ptr<Texture> GenerateNoiseTexture(CommandList& commandList);

private:
    inline static std::uniform_real_distribution<float> s_RandomFloats = std::uniform_real_distribution<float>(0.0f, 1.0f);
    inline static std::default_random_engine s_Generator;
};
