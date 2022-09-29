#pragma once

#include <d3d12.h>

#include <cstdint>
#include <memory>

#include "DescriptorAllocatorPage.h"

/**
 * \brief Single allocation of contiguous descriptors in a descriptor heap.
 * A move-only self-freeing type that is used as a wrapper for D3D12_CPU_DESCRIPTOR_HANDLE.
 * It is move-only to ensure that there is only one instance of each particular allocation.
 */
class DescriptorAllocation
{
public:
	/**
	 * \brief Creates a NULL descriptor.
	 */
	DescriptorAllocation();

	explicit DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, uint32_t numHandles,
		uint32_t descriptorSize, std::shared_ptr<DescriptorAllocatorPage> page);

	/**
	 * \brief Automatically frees the allocation.
	 */
	~DescriptorAllocation();

	// Forbid copies
	DescriptorAllocation(const DescriptorAllocation&) = delete;
	DescriptorAllocation& operator=(const DescriptorAllocation&) = delete;

	// Allow move
	DescriptorAllocation(DescriptorAllocation&& allocation);
	DescriptorAllocation& operator=(DescriptorAllocation&& other);

	/**
	 * \brief Check if descriptor is invalid.
	 * \return True if invalid, false if valid.
	 */
	bool IsNull() const;

	/**
	 * \brief Get a descriptor at a particular offset in the allocation.
	 * \param offset offset
	 * \return descriptor at that offset
	 */
	D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(uint32_t offset = 0) const;

	/**
	 * \brief Get the number of (consecutive) handles for this allocation.
	 * \return number of handles
	 */
	uint32_t GetNumHandles() const;

	/**
	 * \brief Get the heap that this allocation came from (for internal use only).
	 * \return source allocator page
	 */
	std::shared_ptr<DescriptorAllocatorPage> GetDescriptorAllocatorPage() const;

private:
	/**
	 * \brief Free the descriptor back to the heap it came from.
	 */
	void Free();

	D3D12_CPU_DESCRIPTOR_HANDLE Descriptor;
	uint32_t NumHandles;
	uint32_t DescriptorSize;

	std::shared_ptr<DescriptorAllocatorPage> Page;
};
