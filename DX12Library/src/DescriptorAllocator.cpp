#include "DescriptorAllocator.h"

#include "DX12LibPCH.h"

#include "DescriptorAllocator.h"
#include "DescriptorAllocatorPage.h"

DescriptorAllocator::DescriptorAllocator(const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t numDescriptorsPerHeap) :
	HeapType(type),
	NumDescriptorsPerHeap(numDescriptorsPerHeap)
{
}

DescriptorAllocator::~DescriptorAllocator() = default;

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocator::CreateAllocatorPage()
{
	auto newPage = std::make_shared<DescriptorAllocatorPage>(HeapType, NumDescriptorsPerHeap);

	HeapPool.emplace_back(newPage);
	AvailableHeaps.insert(HeapPool.size() - 1);

	return newPage;
}

DescriptorAllocation DescriptorAllocator::Allocate(uint32_t numDescriptors)
{
	std::lock_guard<std::mutex> lock(AllocationMutex);

	DescriptorAllocation allocation;

	for (auto iter = AvailableHeaps.begin(); iter != AvailableHeaps.end(); ++iter)
	{
		const auto allocatorPage = HeapPool[*iter];

		allocation = allocatorPage->Allocate(numDescriptors);

		if (allocatorPage->GetNumFreeHandles() == 0)
		{
			iter = AvailableHeaps.erase(iter);
		}

		// found an allocation
		if (!allocation.IsNull())
		{
			break;
		}
	}

	// Allocation was not possible, no suitable heap was found
	if (allocation.IsNull())
	{
		NumDescriptorsPerHeap = std::max(NumDescriptorsPerHeap, numDescriptors);
		const auto newPage = CreateAllocatorPage();
		allocation = newPage->Allocate(numDescriptors);
	}

	return allocation;
}

void DescriptorAllocator::ReleaseStaleDescriptors(uint64_t frameNumber)
{
	std::lock_guard<std::mutex> lock(AllocationMutex);

	for (size_t i = 0; i < HeapPool.size(); i++)
	{
		const auto page = HeapPool[i];

		page->ReleaseStaleDescriptors(frameNumber);

		if (page->GetNumFreeHandles() > 0)
		{
			// mark as available
			AvailableHeaps.insert(i);
		}
	}
}
