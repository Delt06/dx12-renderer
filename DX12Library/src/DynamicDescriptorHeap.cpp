#include "DX12LibPCH.h"

#include "DynamicDescriptorHeap.h"

#include <stdexcept>

#include "Application.h"
#include "CommandList.h"

#include "RootSignature.h"

DynamicDescriptorHeap::DynamicDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	const uint32_t numDescriptorsPerHeap)
	: m_DescriptorHeapType(heapType)
	, m_NumDescriptorsPerHeap(numDescriptorsPerHeap)
	, m_DescriptorTableBitMask(0)
	, m_StaleDescriptorTableBitMask(0)
	, m_CurrentGpuDescriptorHandle(D3D12_DEFAULT)
	, m_CurrentCpuDescriptorHandle(D3D12_DEFAULT)
	, m_NumFreeHandles(0)
{
	m_DescriptorHandleIncrementSize = Application::Get().GetDescriptorHandleIncrementSize(heapType);

	// Allocate space for CPU descriptors
	m_DescriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(m_NumDescriptorsPerHeap);
}

DynamicDescriptorHeap::~DynamicDescriptorHeap() = default;

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature)
{
	// If the root signature changes, all descriptors must be (re)bound to the
	// command list.

	m_StaleDescriptorTableBitMask = 0;

	const auto& rootSignatureDesc = rootSignature.GetRootSignatureDesc();

	// Get a bit mask that represents the root parameter indices that match the 
	// descriptor heap type for this dynamic descriptor heap.
	m_DescriptorTableBitMask = rootSignature.GetDescriptorTableBitMask(m_DescriptorHeapType);
	uint32_t descriptorTableBitMask = m_DescriptorTableBitMask;

	uint32_t currentOffset = 0;
	DWORD rootIndex;

	while (_BitScanForward(&rootIndex, descriptorTableBitMask) && rootIndex < rootSignatureDesc.NumParameters)
	{
		const uint32_t numDescriptors = rootSignature.GetNumDescriptors(rootIndex);

		DescriptorTableCache& descriptorTableCache = m_DescriptorTableCaches[rootIndex];
		descriptorTableCache.NumDescriptors = numDescriptors;
		descriptorTableCache.BaseDescriptor = m_DescriptorHandleCache.get() + currentOffset;

		currentOffset += numDescriptors;

		descriptorTableBitMask ^= 1 << rootIndex;
	}

	// Make sure the maximum number of descriptors per descriptor heap has not been exceeded.
	assert(
		currentOffset <= m_NumDescriptorsPerHeap &&
		"The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}

void DynamicDescriptorHeap::StageDescriptors(const uint32_t rootParameterIndex, uint32_t offset,
	const uint32_t numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor)
{
	// Cannot stage more than the maximum number of descriptors per heap.
	// Cannot stage more than MaxDescriptorTables root parameters.
	if (numDescriptors > m_NumDescriptorsPerHeap || rootParameterIndex >= MAX_DESCRIPTOR_TABLES)
	{
		throw std::bad_alloc();
	}

	const DescriptorTableCache& descriptorTableCache = m_DescriptorTableCaches[rootParameterIndex];

	// Check that the number of descriptors to copy does not exceed the number
	// of descriptors expected in the descriptor table.
	if ((offset + numDescriptors) > descriptorTableCache.NumDescriptors)
	{
		throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (descriptorTableCache.BaseDescriptor + offset);
	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		dstDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptor, i, m_DescriptorHandleIncrementSize);
	}

	// Set the root parameter index bit to make sure the descriptor table 
	// at that index is bound to the command list.
	m_StaleDescriptorTableBitMask |= (1 << rootParameterIndex);
}

uint32_t DynamicDescriptorHeap::ComputeStaleDescriptorCount() const
{
	uint32_t numStaleDescriptors = 0;
	DWORD i;
	DWORD staleDescriptorsBitMask = m_StaleDescriptorTableBitMask;

	while (_BitScanForward(&i, staleDescriptorsBitMask))
	{
		numStaleDescriptors += m_DescriptorTableCaches[i].NumDescriptors;
		staleDescriptorsBitMask ^= (1 << i);
	}

	return numStaleDescriptors;
}

ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::RequestDescriptorHeap()
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	if (!m_AvailableDescriptorHeaps.empty())
	{
		descriptorHeap = m_AvailableDescriptorHeaps.front();
		m_AvailableDescriptorHeaps.pop();
	}
	else
	{
		descriptorHeap = CreateDescriptorHeap();
		m_DescriptorHeapPool.push(descriptorHeap);
	}

	return descriptorHeap;
}

ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap() const
{
	const auto device = Application::Get().GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = m_DescriptorHeapType;
	descriptorHeapDesc.NumDescriptors = m_NumDescriptorsPerHeap;
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

		if (!m_CurrentDescriptorHeap || m_NumFreeHandles < numDescriptorsToCommit)
		{
			m_CurrentDescriptorHeap = RequestDescriptorHeap();
			m_CurrentCpuDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			m_CurrentGpuDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			m_NumFreeHandles = m_NumDescriptorsPerHeap;

			commandList.SetDescriptorHeap(m_DescriptorHeapType, m_CurrentDescriptorHeap.Get());

			// When updating the descriptor heap on the command list, all descriptor
			// tables must be (re)recopied to the new descriptor heap (not just
			// the stale descriptor tables).
			m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
		}
		DWORD rootIndex;

		while (_BitScanForward(&rootIndex, m_StaleDescriptorTableBitMask))
		{
			const UINT numSrcDescriptors = m_DescriptorTableCaches[rootIndex].NumDescriptors;
			const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorHandles = m_DescriptorTableCaches[rootIndex].BaseDescriptor;

			const D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[] = {
				m_CurrentCpuDescriptorHandle
			};
			const UINT pDestDescriptorRangeSizes[] = {
				numSrcDescriptors
			};

			// Copy the staged CPU visible descriptors to the GPU visible descriptor heap.
			device->CopyDescriptors(1, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes, numSrcDescriptors,
				pSrcDescriptorHandles, nullptr, m_DescriptorHeapType);

			// Set the descriptors on the command list using the passed-in setter function.
			setFunc(graphicsCommandList, rootIndex, m_CurrentGpuDescriptorHandle);

			m_CurrentCpuDescriptorHandle.Offset(static_cast<INT>(numSrcDescriptors), m_DescriptorHandleIncrementSize);
			m_CurrentGpuDescriptorHandle.Offset(static_cast<INT>(numSrcDescriptors), m_DescriptorHandleIncrementSize);
			m_NumFreeHandles -= numSrcDescriptors;

			// Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor.
			m_StaleDescriptorTableBitMask ^= (1 << rootIndex);
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
	if (!m_CurrentDescriptorHeap || m_NumFreeHandles < 1)
	{
		m_CurrentDescriptorHeap = RequestDescriptorHeap();
		m_CurrentCpuDescriptorHandle = m_CurrentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		m_CurrentGpuDescriptorHandle = m_CurrentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		m_NumFreeHandles = m_NumDescriptorsPerHeap;

		commandList.SetDescriptorHeap(m_DescriptorHeapType, m_CurrentDescriptorHeap.Get());

		// When updating the descriptor heap on the command list, all descriptor
		// tables must be (re)recopied to the new descriptor heap (not just
		// the stale descriptor tables).
		m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
	}

	const auto device = Application::Get().GetDevice();

	const CD3DX12_GPU_DESCRIPTOR_HANDLE hGpu = m_CurrentGpuDescriptorHandle;
	device->CopyDescriptorsSimple(1, m_CurrentCpuDescriptorHandle, cpuDescriptor, m_DescriptorHeapType);

	m_CurrentCpuDescriptorHandle.Offset(1, m_DescriptorHandleIncrementSize);
	m_CurrentGpuDescriptorHandle.Offset(1, m_DescriptorHandleIncrementSize);
	m_NumFreeHandles -= 1;

	return hGpu;
}

void DynamicDescriptorHeap::Reset()
{
	m_AvailableDescriptorHeaps = m_DescriptorHeapPool;
	m_CurrentDescriptorHeap.Reset();
	m_CurrentCpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	m_CurrentGpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	m_NumFreeHandles = 0;
	m_DescriptorTableBitMask = 0;
	m_StaleDescriptorTableBitMask = 0;

	for (auto& descriptorTableCache : m_DescriptorTableCaches)
	{
		descriptorTableCache.Reset();
	}
}
