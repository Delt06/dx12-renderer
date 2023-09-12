#include "TransientResourceAllocator.h"

#include "RenderPass.h"
#include "ResourceDescription.h"

using namespace RenderGraph;

namespace
{
    TransientResourceAllocator::ResourceLifecycle& GetOrAdd(std::map<ResourceId, TransientResourceAllocator::ResourceLifecycle>& map, const ResourceId& id, uint32_t passIndex)
    {
        if (!map.contains(id))
        {
            map.insert(std::pair<ResourceId, TransientResourceAllocator::ResourceLifecycle> {id, { id, passIndex, passIndex }});
        }

        return map[id];
    }
}

std::map<ResourceId, TransientResourceAllocator::ResourceLifecycle> TransientResourceAllocator::GetResourceLifecycles(const std::vector<RenderPass*>& renderPasses)
{
    std::map<ResourceId, TransientResourceAllocator::ResourceLifecycle> lifecycles;

    for (uint32_t passIndex = 0; passIndex < renderPasses.size(); ++passIndex)
    {
        const auto& pass = *renderPasses[passIndex];

        for (const auto& output : pass.GetOutputs())
        {
            auto& lifecycle = GetOrAdd(lifecycles, output.m_Id, passIndex);
            lifecycle.m_EndPassIndex = passIndex;
        }

        for (const auto& input : pass.GetInputs())
        {
            auto& lifecycle = GetOrAdd(lifecycles, input.m_Id, passIndex);
            Assert(lifecycle.m_BeginPassIndex != passIndex, "A resource's first usage cannot be as an input.");

            lifecycle.m_EndPassIndex = passIndex;
        }
    }

    {
        const uint32_t lastPassIndex = static_cast<uint32_t>(renderPasses.size()) - 1;
        auto& graphOutputLifecycle = GetOrAdd(lifecycles, ResourceIds::GraphOutput, lastPassIndex);
        graphOutputLifecycle.m_EndPassIndex = lastPassIndex;
    }

    return lifecycles;
}


std::vector<TransientResourceAllocator::HeapInfo> TransientResourceAllocator::CreateHeaps(const std::map<ResourceId, TransientResourceAllocator::ResourceLifecycle>& lifecycles, const std::map<ResourceId, ResourceDescription>& resourceDescriptions, const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice)
{
    std::vector<TransientResourceAllocator::HeapInfo> heaps;

    for (const auto& [id, lifecycle] : lifecycles)
    {
        const auto findResult = resourceDescriptions.find(id);

        // actually used resources should always be registered at this point
        if (findResult != resourceDescriptions.end())
        {
            HeapInfo heapInfo;
            heapInfo.m_Size = findResult->second.m_TotalSize;
            heapInfo.m_Alignment = findResult->second.m_Alignment;
            heapInfo.m_ResourceLifecycles.push_back(lifecycle);

            heaps.emplace_back(std::move(heapInfo));
        }
    }

    bool compacting;

    do
    {
        compacting = false;

        for (uint32_t expandingIndex = 0; expandingIndex < heaps.size() - 1; ++expandingIndex)
        {
            auto& expandingHeap = heaps[expandingIndex];
            uint32_t theBiggestFittingHeapIndex = -1;
            uint64_t theBiggestFittingHeapSize = 0;

            for (uint32_t otherIndex = expandingIndex + 1; otherIndex < heaps.size(); ++otherIndex)
            {
                const auto& otherHeap = heaps[otherIndex];
                if (otherHeap.m_Size <= expandingHeap.m_Size &&
                    otherHeap.m_Alignment == expandingHeap.m_Alignment &&
                    otherHeap.m_ResourceLifecycles.size() == 1
                    )
                {
                    const auto& otherLifecycle = otherHeap.m_ResourceLifecycles[0];

                    bool intersect = false;

                    for (const auto& expandingLifecycle : expandingHeap.m_ResourceLifecycles)
                    {
                        if (ResourceLifecycle::Intersect(expandingLifecycle, otherLifecycle))
                        {
                            intersect = true;
                            break;
                        }
                    }

                    if (!intersect)
                    {
                        if (theBiggestFittingHeapIndex == -1 || otherHeap.m_Size > theBiggestFittingHeapSize)
                        {
                            theBiggestFittingHeapIndex = otherIndex;
                            theBiggestFittingHeapSize = otherHeap.m_Size;
                        }
                    }
                }
            }

            if (theBiggestFittingHeapIndex != -1)
            {
                {
                    const auto& otherHeap = heaps[theBiggestFittingHeapIndex];
                    expandingHeap.m_ResourceLifecycles.push_back(otherHeap.m_ResourceLifecycles[0]);
                }

                heaps.erase(heaps.begin() + theBiggestFittingHeapIndex);
                compacting = true;
                break;
            }
        }

    }
    while (compacting);

    for (uint32_t i = 0; i < heaps.size(); ++i)
    {
        auto& heapInfo = heaps[i];
        const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = { heapInfo.m_Size, heapInfo.m_Alignment };
        const auto heapDesc = CD3DX12_HEAP_DESC(allocationInfo, D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE);

        ThrowIfFailed(pDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(&heapInfo.m_Heap)));

        const auto name = L"RenderGraph-TransientResourceHeap-" + std::to_wstring(i);
        heapInfo.m_Heap->SetName(name.c_str());
    }

    return heaps;
}


