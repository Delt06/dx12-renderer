#include "DX12LibPCH.h"

#include "DescriptorAllocatorPage.h"
#include "Application.h"

DescriptorAllocatorPage::DescriptorAllocatorPage(const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t numDescriptors) :
	FreeListByOffset(),
	FreeListBySize(),
	HeapType(type),
	NumDescriptorsInHeap(numDescriptors)
{
	const auto device = Application::Get().GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = HeapType;
	heapDesc.NumDescriptors = NumDescriptorsInHeap;

	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&DescriptorHeap)));

	BaseDescriptor = DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	DescriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(HeapType);
	NumFreeHandles = NumDescriptorsInHeap;

	// Initialize the free lists
	AddNewBlock(0, NumFreeHandles);
}

D3D12_DESCRIPTOR_HEAP_TYPE DescriptorAllocatorPage::GetHeapType() const
{
	return HeapType;
}

uint32_t DescriptorAllocatorPage::GetNumFreeHandles() const
{
	return NumFreeHandles;
}

bool DescriptorAllocatorPage::HasSpace(const uint32_t numDescriptors)
{
	// lower_bounds searches for the first entry that is >= numDescriptors
	// if none is found, it returns the past-the-end iterator
	return FreeListBySize.lower_bound(numDescriptors) != FreeListBySize.end();
}

void DescriptorAllocatorPage::AddNewBlock(uint32_t offset, uint32_t numDescriptors)
{
	auto offsetIter = FreeListByOffset.emplace(offset, numDescriptors);
	const auto sizeIter = FreeListBySize.emplace(numDescriptors, offsetIter.first);
	offsetIter.first->second.FreeListBySizeIter = sizeIter;
}

DescriptorAllocation DescriptorAllocatorPage::Allocate(const uint32_t numDescriptors)
{
	std::lock_guard<std::mutex> lock(AllocationMutex);

	if (numDescriptors > NumFreeHandles)
	{
		// insufficient amount of space, return a null allocation
		return {};
	}

	// smallest block with sufficient space
	const auto smallestBlockIter = FreeListBySize.lower_bound(numDescriptors);
	if (smallestBlockIter == FreeListBySize.end())
	{
		// such block not found
		return {};
	}

	const auto blockSize = smallestBlockIter->first;
	const auto offsetIter = smallestBlockIter->second;
	const auto offset = offsetIter->first;

	// Remove the block from the free list
	FreeListBySize.erase(smallestBlockIter);
	FreeListByOffset.erase(offsetIter);

	// Compute the new free block that results from splitting
	const auto newOffset = offset + numDescriptors;
	const auto newSize = blockSize - numDescriptors;

	if (newSize > 0)
	{
		AddNewBlock(newOffset, newSize);
	}

	// Decrement free handles
	NumFreeHandles -= numDescriptors;

	return DescriptorAllocation(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(BaseDescriptor, offset, DescriptorHandleIncrementSize),
		numDescriptors, DescriptorHandleIncrementSize, shared_from_this()
	);
}

uint32_t DescriptorAllocatorPage::ComputeOffset(const D3D12_CPU_DESCRIPTOR_HANDLE handle) const
{
	return static_cast<uint32_t>(handle.ptr - BaseDescriptor.ptr) / DescriptorHandleIncrementSize;
}

void DescriptorAllocatorPage::Free(DescriptorAllocation&& descriptor, uint64_t frameNumber)
{
	auto offset = ComputeOffset(descriptor.GetDescriptorHandle());
	std::lock_guard<std::mutex> lock(AllocationMutex);

	StaleDescriptors.emplace(offset, descriptor.GetNumHandles(), frameNumber);
}

void DescriptorAllocatorPage::FreeBlock(uint32_t offset, uint32_t numDescriptors)
{
	// Find the element right after the block being freed
	const auto nextBlockIter = FreeListByOffset.upper_bound(offset);

	// ...before the block being freed
	auto prevBlockIter = nextBlockIter;
	if (prevBlockIter != FreeListByOffset.begin())
	{
		--prevBlockIter;
	}
	else
	{
		// there is no such block
		prevBlockIter = FreeListByOffset.end();
	}

	NumFreeHandles += numDescriptors;

	if (prevBlockIter != FreeListByOffset.end() &&
		offset == prevBlockIter->first + prevBlockIter->second.Size)
	{
		// The previous block is exactly behind the block that is to be freed

		// increase the block size
		offset = prevBlockIter->first;
		numDescriptors += prevBlockIter->second.Size;

		// remove the previous block from the free list
		FreeListBySize.erase(prevBlockIter->second.FreeListBySizeIter);
		FreeListByOffset.erase(prevBlockIter);
	}

	if (nextBlockIter != FreeListByOffset.end() &&
		offset + numDescriptors == nextBlockIter->first)
	{
		// The next block is exactly in front of the block that is to be freed

		// increase the block size
		numDescriptors += nextBlockIter->second.Size;

		// remove the next block from the free list
		FreeListBySize.erase(nextBlockIter->second.FreeListBySizeIter);
		FreeListByOffset.erase(nextBlockIter);
	}

	// Add the block to the free list
	AddNewBlock(offset, numDescriptors);
}

void DescriptorAllocatorPage::ReleaseStaleDescriptors(const uint64_t frameNumber)
{
	std::lock_guard<std::mutex> lock(AllocationMutex);

	while (!StaleDescriptors.empty() && StaleDescriptors.front().FrameNumber <= frameNumber)
	{
		const auto staleDescriptor = StaleDescriptors.front();
		const auto offset = staleDescriptor.Offset;
		const auto numDescriptors = staleDescriptor.Size;

		FreeBlock(offset, numDescriptors);

		StaleDescriptors.pop();
	}
}
