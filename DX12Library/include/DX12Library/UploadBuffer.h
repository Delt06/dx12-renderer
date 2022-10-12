#pragma once

#include "Defines.h"

#include <wrl.h>
#include <d3d12.h>

#include <memory>
#include <deque>

class UploadBuffer
{
public:
	// Use to upload data to the GPU
	struct Allocation
	{
		void* Cpu;
		D3D12_GPU_VIRTUAL_ADDRESS Gpu;
	};

	/**
	 * \param pageSize The size to use to allocate new pages in GPU memory.
	 */
	explicit UploadBuffer(size_t pageSize = _2MB);


	size_t GetPageSize() const { return m_PageSize; }

	/**
	 * Allocate memory in an Upload heap.
	 * An allocation must not exceed the size of a page.
	 * Use a memcpy or similar method to copy the
	 * buffer data to CPU pointer in the Allocation structure returned from
	 * this function.
	 */
	Allocation Allocate(size_t sizeInBytes, size_t alignment);

	/**
	 * \brief Release all allocated pages.
	 * This should only be called when the command list is finished executing on the CommandQueue.
	 */
	void Reset();

private:
	struct Page
	{
		explicit Page(size_t sizeInBytes);
		~Page();

		bool HasSpace(size_t sizeInBytes, size_t alignment) const;

		// Allocate memory from the page.
		// Throws std::bad_alloc if the the allocation size is larger
		// that the page size or the size of the allocation exceeds the 
		// remaining space in the page.
		Allocation Allocate(size_t sizeInBytes, size_t alignment);

		void Reset();

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource;

		void* m_CpuPtr;
		D3D12_GPU_VIRTUAL_ADDRESS m_GpuPtr;

		size_t m_SizeInBytes;
		size_t m_OffsetInBytes;
	};

	using PagePoolType = std::deque<std::shared_ptr<Page>>;

	/**
	 * \brief Get an available page or create a new one
	 */
	std::shared_ptr<Page> RequestPage();

	PagePoolType m_PagePool;
	PagePoolType m_AvailablePages;

	std::shared_ptr<Page> m_CurrentPage;
	size_t m_PageSize;
};
