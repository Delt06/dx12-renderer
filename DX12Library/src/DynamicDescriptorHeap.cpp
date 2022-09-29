#include "DX12LibPCH.h"

#include "DynamicDescriptorHeap.h"

#include <stdexcept>

#include "Application.h"
#include "CommandList.h"

#include "RootSignature.h"

DynamicDescriptorHeap::DynamicDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	const uint32_t numDescriptorsPerHeap)
	: DescriptorHeapType(heapType)
	, NumDescriptorsPerHeap(numDescriptorsPerHeap)
	, DescriptorTableBitMask(0)
	, StaleDescriptorTableBitMask(0)
	, CurrentGpuDescriptorHandle(D3D12_DEFAULT)
	, CurrentCpuDescriptorHandle(D3D12_DEFAULT)
	, NumFreeHandles(0)
{
	DescriptorHandleIncrementSize = Application::Get().GetDescriptorHandleIncrementSize(heapType);

	// Allocate space for CPU descriptors
	DescriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(NumDescriptorsPerHeap);
}

DynamicDescriptorHeap::~DynamicDescriptorHeap() = default;

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature)
{
	// If the root signature changes, all descriptors must be (re)bound to the
	// command list.

	StaleDescriptorTableBitMask = 0;

	const auto& rootSignatureDesc = rootSignature.GetRootSignatureDesc();

	// Get a bit mask that represents the root parameter indices that match the 
	// descriptor heap type for this dynamic descriptor heap.
	DescriptorTableBitMask = rootSignature.GetDescriptorTableBitMask(DescriptorHeapType);
	uint32_t descriptorTableBitMask = DescriptorTableBitMask;

	uint32_t currentOffset = 0;
	DWORD rootIndex;

	while (_BitScanForward(&rootIndex, descriptorTableBitMask) && rootIndex < rootSignatureDesc.NumParameters)
	{
		const uint32_t numDescriptors = rootSignature.GetNumDescriptors(rootIndex);

		DescriptorTableCache& descriptorTableCache = DescriptorTableCaches[rootIndex];
		descriptorTableCache.NumDescriptors = numDescriptors;
		descriptorTableCache.BaseDescriptor = DescriptorHandleCache.get() + currentOffset;

		currentOffset += numDescriptors;

		descriptorTableBitMask ^= 1 << rootIndex;
	}

	// Make sure the maximum number of descriptors per descriptor heap has not been exceeded.
	assert(
		currentOffset <= NumDescriptorsPerHeap &&
		"The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}

void DynamicDescriptorHeap::StageDescriptors(const uint32_t rootParameterIndex, uint32_t offset,
	const uint32_t numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor)
{
	// Cannot stage more than the maximum number of descriptors per heap.
	// Cannot stage more than MaxDescriptorTables root parameters.
	if (numDescriptors > NumDescriptorsPerHeap || rootParameterIndex >= MAX_DESCRIPTOR_TABLES)
	{
		throw std::bad_alloc();
	}

	const DescriptorTableCache& descriptorTableCache = DescriptorTableCaches[rootParameterIndex];

	// Check that the number of descriptors to copy does not exceed the number
	// of descriptors expected in the descriptor table.
	if ((offset + numDescriptors) > descriptorTableCache.NumDescriptors)
	{
		throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (descriptorTableCache.BaseDescriptor + offset);
	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		dstDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptor, i, DescriptorHandleIncrementSize);
	}

	// Set the root parameter index bit to make sure the descriptor table 
	// at that index is bound to the command list.
	StaleDescriptorTableBitMask |= (1 << rootParameterIndex);
}

uint32_t DynamicDescriptorHeap::ComputeStaleDescriptorCount() const
{
	uint32_t numStaleDescriptors = 0;
	DWORD i;
	DWORD staleDescriptorsBitMask = StaleDescriptorTableBitMask;

	while (_BitScanForward(&i, staleDescriptorsBitMask))
	{
		numStaleDescriptors += DescriptorTableCaches[i].NumDescriptors;
		staleDescriptorsBitMask ^= (1 << i);
	}

	return numStaleDescriptors;
}

ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::RequestDescriptorHeap()
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	if (!AvailableDescriptorHeaps.empty())
	{
		descriptorHeap = AvailableDescriptorHeaps.front();
		AvailableDescriptorHeaps.pop();
	}
	else
	{
		descriptorHeap = CreateDescriptorHeap();
		DescriptorHeapPool.push(descriptorHeap);
	}

	return descriptorHeap;
}

ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap() const
{
	const auto device = Application::Get().GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = DescriptorHeapType;
	descriptorHeapDesc.NumDescriptors = NumDescriptorsPerHeap;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	ThrowIfFailed(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

void DynamicDescriptorHeap::CommitStagedDescriptors(CommandList& commandList,
	std::function<void(ID3D12GraphicsCommandList*, UINT,
		D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc)
{
	uint32_t numDescriptorsToCommit = ComputeStaleDescriptorCount();

	if (numDescriptorsToCommit > 0)
	{
		auto device = Application::Get().GetDevice();
		auto graphicsCommandList = commandList.GetGraphicsCommandList().Get();
		assert(graphicsCommandList != nullptr);

		if (!CurrentDescriptorHeap || NumFreeHandles < numDescriptorsToCommit)
		{
			CurrentDescriptorHeap = RequestDescriptorHeap();
			CurrentCpuDescriptorHandle = CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			CurrentGpuDescriptorHandle = CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			NumFreeHandles = NumDescriptorsPerHeap;

			commandList.SetDescriptorHeap(DescriptorHeapType, CurrentDescriptorHeap.Get());

			// When updating the descriptor heap on the command list, all descriptor
			// tables must be (re)recopied to the new descriptor heap (not just
			// the stale descriptor tables).
			StaleDescriptorTableBitMask = DescriptorTableBitMask;
		}
		DWORD rootIndex;

		while (_BitScanForward(&rootIndex, StaleDescriptorTableBitMask))
		{
			const UINT numSrcDescriptors = DescriptorTableCaches[rootIndex].NumDescriptors;
			const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorHandles = DescriptorTableCaches[rootIndex].
				BaseDescriptor;

			const D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[] = {
				CurrentCpuDescriptorHandle
			};
			const UINT pDestDescriptorRangeSizes[] = {
				numSrcDescriptors
			};

			// Copy the staged CPU visible descriptors to the GPU visible descriptor heap.
			device->CopyDescriptors(1, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes, numSrcDescriptors,
				pSrcDescriptorHandles, nullptr, DescriptorHeapType);

			// Set the descriptors on the command list using the passed-in setter function.
			setFunc(graphicsCommandList, rootIndex, CurrentGpuDescriptorHandle);

			CurrentCpuDescriptorHandle.Offset(static_cast<INT>(numSrcDescriptors), DescriptorHandleIncrementSize);
			CurrentGpuDescriptorHandle.Offset(static_cast<INT>(numSrcDescriptors), DescriptorHandleIncrementSize);

			// Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor.
			StaleDescriptorTableBitMask ^= (1 << rootIndex);
		}
	}
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDraw(CommandList& commandList)
{
	CommitStagedDescriptors(commandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDispatch(CommandList& commandList)
{
	CommitStagedDescriptors(commandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(CommandList& commandList,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
	if (!CurrentDescriptorHeap || NumFreeHandles < 1)
	{
		CurrentDescriptorHeap = RequestDescriptorHeap();
		CurrentCpuDescriptorHandle = CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		CurrentGpuDescriptorHandle = CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		NumFreeHandles = NumDescriptorsPerHeap;

		commandList.SetDescriptorHeap(DescriptorHeapType, CurrentDescriptorHeap.Get());

		// When updating the descriptor heap on the command list, all descriptor
		// tables must be (re)recopied to the new descriptor heap (not just
		// the stale descriptor tables).
		StaleDescriptorTableBitMask = DescriptorTableBitMask;
	}

	const auto device = Application::Get().GetDevice();

	const CD3DX12_GPU_DESCRIPTOR_HANDLE hGpu = CurrentGpuDescriptorHandle;
	device->CopyDescriptorsSimple(1, CurrentCpuDescriptorHandle, cpuDescriptor, DescriptorHeapType);

	CurrentCpuDescriptorHandle.Offset(1, DescriptorHandleIncrementSize);
	CurrentGpuDescriptorHandle.Offset(1, DescriptorHandleIncrementSize);
	NumFreeHandles -= 1;

	return hGpu;
}

void DynamicDescriptorHeap::Reset()
{
	AvailableDescriptorHeaps = DescriptorHeapPool;
	CurrentDescriptorHeap.Reset();
	CurrentCpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	CurrentGpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	NumFreeHandles = 0;
	DescriptorTableBitMask = 0;
	StaleDescriptorTableBitMask = 0;

	for (auto& descriptorTableCache : DescriptorTableCaches)
	{
		descriptorTableCache.Reset();
	}
}
