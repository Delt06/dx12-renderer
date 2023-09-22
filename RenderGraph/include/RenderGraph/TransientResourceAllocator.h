#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include <wrl.h>

#include <DX12Library/Helpers.h>

#include "ResourceId.h"

namespace RenderGraph
{
    class RenderPass;
    struct ResourceDescription;

    class TransientResourceAllocator
    {
    public:
        struct ResourceLifecycle
        {
            ResourceId m_Id;
            uint32_t m_BeginPassIndex;
            uint32_t m_EndPassIndex;

            static bool Intersect(const ResourceLifecycle& lifecycle1, const ResourceLifecycle& lifecycle2);
        };

        struct HeapInfo
        {
            uint64_t m_Size = 0;
            uint64_t m_Alignment = 0;
            std::vector<ResourceLifecycle> m_ResourceLifecycles;
            Microsoft::WRL::ComPtr<ID3D12Heap> m_Heap;
        };

        static std::map<ResourceId, ResourceLifecycle> GetResourceLifecycles(const std::vector<RenderPass*>& renderPasses);
        static std::vector<HeapInfo> CreateHeaps(const std::map<ResourceId, ResourceLifecycle>& lifecycles, const std::map<ResourceId, ResourceDescription>& resourceDescriptions, const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice);
    };
}
