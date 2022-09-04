#pragma once

#include "DescriptorAllocation.h"

#include "d3dx12.h"

#include <cstdint>
#include <mutex>
#include <memory>
#include <set>
#include <vector>

class DescriptorAllocatorPage;

/*
 * Allocates descriptors in a CPU visible heap for resources (thread safe)
 */
class DescriptorAllocator
{
public:
	explicit DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerHeap = 256);
	virtual ~DescriptorAllocator();


	/**
	 * \brief Allocate a number of contiguous descriptors from a CPU visible descriptor heap.
	 *
	 * @param numDescriptors The number of contiguous descriptors to allocate.
	 * Cannot be more than the number of descriptors per descriptor heap.
	 */
	DescriptorAllocation Allocate(uint32_t numDescriptors = 1);

	/**
	 * \brief When the frame has completed, the stale descriptors can be released.
	 */
	void ReleaseStaleDescriptors(uint64_t frameNumber);

private:
	using DescriptorHeapPoolType = std::vector<std::shared_ptr<DescriptorAllocatorPage>>;

	/**
	 * \brief Create a new heap with a specific number of descriptors.
	 * \return new page
	 */
	std::shared_ptr<DescriptorAllocatorPage> CreateAllocatorPage();

	D3D12_DESCRIPTOR_HEAP_TYPE HeapType;
	uint32_t NumDescriptorsPerHeap;

	DescriptorHeapPoolType HeapPool;
	// Indices of available heaps (in the pool)
	std::set<size_t> AvailableHeaps;

	std::mutex AllocationMutex;
};
