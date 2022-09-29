#pragma once

#include <d3d12.h>
#include "d3dx12.h"


#include <wrl.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>

#include "RootSignature.h"

class CommandList;
class RootSignature;

/**
 * \brief Allocates GPU visible descriptors to bind CBV, SRV, UAV, and Samples to GPU pipeline.
 * Heavily based on https://github.com/Microsoft/DirectX-Graphics-Samples
 */
class DynamicDescriptorHeap
{
public:
	DynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptorsPerHeap = 1024);

	virtual ~DynamicDescriptorHeap();

	/**
	 * Stages a contiguous range of CPU visible descriptors.
	 * Descriptors are not copied to the GPU visible descriptor heap until
	 * the CommitStagedDescriptors function is called.
	 */
	void StageDescriptors(uint32_t rootParameterIndex, uint32_t offset, uint32_t numDescriptors,
		D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor);


	/**
	 * Copy all of the staged descriptors to the GPU visible descriptor heap and
	 * bind the descriptor heap and the descriptor tables to the command list.
	 * The passed-in function object is used to set the GPU visible descriptors
	 * on the command list. Two possible functions are:
	 *   * Before a draw    : ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable
	 *   * Before a dispatch: ID3D12GraphicsCommandList::SetComputeRootDescriptorTable
	 *
	 * Since the DynamicDescriptorHeap can't know which function will be used, it must
	 * be passed as an argument to the function.
	 */
	void CommitStagedDescriptors(CommandList& commandList,
		std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)>
		setFunc);
	void CommitStagedDescriptorsForDraw(CommandList& commandList);
	void CommitStagedDescriptorsForDispatch(CommandList& commandList);

	/**
	 * Copies a single CPU visible descriptor to a GPU visible descriptor heap.
	 * This is useful for the
	 *   * ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat
	 *   * ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint
	 * methods which require both a CPU and GPU visible descriptors for a UAV
	 * resource.
	 *
	 * @param commandList The command list is required in case the GPU visible
	 * descriptor heap needs to be updated on the command list.
	 * @param cpuDescriptor The CPU descriptor to copy into a GPU visible
	 * descriptor heap.
	 *
	 * @return The GPU visible descriptor.
	 */
	D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(CommandList& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor);

	/**
	 * Parse the root signature to determine which root parameters contain
	 * descriptor tables and determine the number of descriptors needed for
	 * each table.
	 */
	void ParseRootSignature(const RootSignature& rootSignature);

	/**
	 * Reset used descriptors. This should only be done if any descriptors
	 * that are being referenced by a command list has finished executing on the
	 * command queue.
	 */
	void Reset();

private:
	// Request a descriptor heap if one is available.
	ComPtr<ID3D12DescriptorHeap> RequestDescriptorHeap();
	// Create a new descriptor heap of no descriptor heap is available.
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap() const;

	// Compute the number of stale descriptors that need to be copied
	// to GPU visible descriptor heap.
	uint32_t ComputeStaleDescriptorCount() const;

	/**
	 * The maximum number of descriptor tables per root signature.
	 * A 32-bit mask is used to keep track of the root parameter indices that
	 * are descriptor tables.
	 */
	static constexpr size_t MAX_DESCRIPTOR_TABLES = RootSignature::MAX_DESCRIPTOR_TABLES;

	// A structure that represents a descriptor table entry in the root signature.
	struct DescriptorTableCache
	{
		DescriptorTableCache() :
			NumDescriptors(0),
			BaseDescriptor(nullptr)
		{
		}

		void Reset()
		{
			NumDescriptors = 0;
			BaseDescriptor = nullptr;
		}

		uint32_t NumDescriptors;
		D3D12_CPU_DESCRIPTOR_HANDLE* BaseDescriptor;
	};

	// Describes the type of descriptors that can be staged using this 
	// dynamic descriptor heap.
	// Valid values are:
	//   * D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	//   * D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
	// This parameter also determines the type of GPU visible descriptor heap to create.
	D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType;

	uint32_t NumDescriptorsPerHeap;
	uint32_t DescriptorHandleIncrementSize;

	std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> DescriptorHandleCache;
	DescriptorTableCache DescriptorTableCaches[MAX_DESCRIPTOR_TABLES];

	uint32_t DescriptorTableBitMask;
	uint32_t StaleDescriptorTableBitMask;

	using DescriptorHeapPoolType = std::queue<ComPtr<ID3D12DescriptorHeap>>;

	DescriptorHeapPoolType DescriptorHeapPool;
	DescriptorHeapPoolType AvailableDescriptorHeaps;

	ComPtr<ID3D12DescriptorHeap> CurrentDescriptorHeap;
	CD3DX12_GPU_DESCRIPTOR_HANDLE CurrentGpuDescriptorHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE CurrentCpuDescriptorHandle;

	uint32_t NumFreeHandles;
};
