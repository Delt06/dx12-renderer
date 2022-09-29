#pragma once

#include <d3d12.h>

#include <wrl.h>

#include <map>
#include <memory>
#include <mutex>
#include <queue>

class DescriptorAllocation;

class DescriptorAllocatorPage : public std::enable_shared_from_this<DescriptorAllocatorPage>
{
public:
	explicit DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);

	D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const;

	bool HasSpace(uint32_t numDescriptors);

	/**
	 * \brief Get the number of available handles in the heap.
	 * \return The number of free handles.
	 */
	uint32_t GetNumFreeHandles() const;

	/**
	 * \brief Allocate a number of descriptors from this descriptor heap.
	 * \param numDescriptors descriptors to allocate
	 * \return Allocation if success, Null Allocation if could not be satisfied.
	 */
	DescriptorAllocation Allocate(uint32_t numDescriptors);

	/**
	 * \brief Return a descriptor back to the heap (mark for further return).
	 * \param descriptor descriptor to return.
	 * \param frameNumber used to identify the descriptors later. They are not freed up immediately, but rather put on a stale allocations queue.
	 * Stale descriptors are returned to the heap when using ReleaseStaleAllocations
	 */
	void Free(DescriptorAllocation&& descriptor, uint64_t frameNumber);

	/**
	 * Returned the stale descriptors back to the descriptor heap.
	 */
	void ReleaseStaleDescriptors(uint64_t frameNumber);

protected:
	/**
	 * \brief Compute offset of the descriptor from the start of the type
	 * \return offset
	 */
	uint32_t ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle) const;

	/**
	 * \brief Add a new block to the free list
	 * \param offset offset
	 * \param numDescriptors number of descriptors (block size)
	 */
	void AddNewBlock(uint32_t offset, uint32_t numDescriptors);

	/**
	 * \brief Free a block of descriptors. This will also merge free blocks in the free list.
	 * \param offset offset
	 * \param numDescriptors number of descriptors (block size)
	 */
	void FreeBlock(uint32_t offset, uint32_t numDescriptors);

private:
	using OffsetType = uint32_t;
	using SizeType = uint32_t;

	struct FreeBlockInfo;

	using FreeListByOffsetType = std::map<OffsetType, FreeBlockInfo>;
	using FreeListBySizeType = std::multimap<SizeType, FreeListByOffsetType::iterator>;

	struct FreeBlockInfo
	{
		explicit FreeBlockInfo(const SizeType size) : Size(size)
		{
		}

		SizeType Size;
		FreeListBySizeType::iterator FreeListBySizeIter{};
	};

	struct StaleDescriptorInfo
	{
		StaleDescriptorInfo(const OffsetType offset, const SizeType size, const uint64_t frameNumber) : Offset(offset),
			Size(size), FrameNumber(frameNumber)
		{
		}

		OffsetType Offset;
		SizeType Size;
		uint64_t FrameNumber;
	};

	// Stale descriptors are queued for release until the frame that they were freed
	// has completed.
	using StaleDescriptorQueueType = std::queue<StaleDescriptorInfo>;

	FreeListByOffsetType FreeListByOffset;
	FreeListBySizeType FreeListBySize;
	StaleDescriptorQueueType StaleDescriptors;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE HeapType;
	D3D12_CPU_DESCRIPTOR_HANDLE BaseDescriptor;
	uint32_t DescriptorHandleIncrementSize;
	uint32_t NumDescriptorsInHeap;
	uint32_t NumFreeHandles;

	std::mutex AllocationMutex;
};
