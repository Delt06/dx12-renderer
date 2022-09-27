#include "DX12LibPCH.h"

#include "RootSignature.h"

#include "Application.h"

RootSignature::RootSignature()
	:
	RootSignatureDesc{}
	, NumDescriptorsPerTable{0}
	, SamplerTableBitMask(0)
	, DescriptorTableBitMask(0)
{
}

RootSignature::RootSignature(
	const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc, const D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion)
	: RootSignatureDesc{}
	  , NumDescriptorsPerTable{0}
	  , SamplerTableBitMask(0)
	  , DescriptorTableBitMask(0)
{
	SetRootSignatureDesc(rootSignatureDesc, rootSignatureVersion);
}

RootSignature::~RootSignature()
{
	Destroy();
}

void RootSignature::Destroy()
{
	for (UINT i = 0; i < RootSignatureDesc.NumParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& rootParameter = RootSignatureDesc.pParameters[i];
		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			delete[] rootParameter.DescriptorTable.pDescriptorRanges;
		}
	}

	delete[] RootSignatureDesc.pParameters;
	RootSignatureDesc.pParameters = nullptr;
	RootSignatureDesc.NumParameters = 0;

	delete[] RootSignatureDesc.pStaticSamplers;
	RootSignatureDesc.pStaticSamplers = nullptr;
	RootSignatureDesc.NumStaticSamplers = 0;

	DescriptorTableBitMask = 0;
	SamplerTableBitMask = 0;

	memset(NumDescriptorsPerTable, 0, sizeof NumDescriptorsPerTable);
}

void RootSignature::SetRootSignatureDesc(
	const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc,
	const D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion
)
{
	// Make sure any previously allocated root signature description is cleaned 
	// up first.
	Destroy();

	const auto device = Application::Get().GetDevice();

	UINT numParameters = rootSignatureDesc.NumParameters;
	D3D12_ROOT_PARAMETER1* pParameters = numParameters > 0 ? new D3D12_ROOT_PARAMETER1[numParameters] : nullptr;

	for (UINT i = 0; i < numParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& rootParameter = rootSignatureDesc.pParameters[i];
		pParameters[i] = rootParameter;

		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			const UINT numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
			D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges = numDescriptorRanges > 0
				                                             ? new D3D12_DESCRIPTOR_RANGE1[numDescriptorRanges]
				                                             : nullptr;

			memcpy(pDescriptorRanges, rootParameter.DescriptorTable.pDescriptorRanges,
			       sizeof(D3D12_DESCRIPTOR_RANGE1) * numDescriptorRanges);

			pParameters[i].DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
			pParameters[i].DescriptorTable.pDescriptorRanges = pDescriptorRanges;

			// Set the bit mask depending on the type of descriptor table.
			if (numDescriptorRanges > 0)
			{
				switch (pDescriptorRanges[0].RangeType)
				{
				case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
				case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
				case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
					DescriptorTableBitMask |= (1 << i);
					break;
				case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
					SamplerTableBitMask |= (1 << i);
					break;
				}
			}

			// Count the number of descriptors in the descriptor table.
			for (UINT j = 0; j < numDescriptorRanges; ++j)
			{
				NumDescriptorsPerTable[i] += pDescriptorRanges[j].NumDescriptors;
			}
		}
	}

	RootSignatureDesc.NumParameters = numParameters;
	RootSignatureDesc.pParameters = pParameters;

	UINT numStaticSamplers = rootSignatureDesc.NumStaticSamplers;
	D3D12_STATIC_SAMPLER_DESC* pStaticSamplers = numStaticSamplers > 0
		                                             ? new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers]
		                                             : nullptr;

	if (pStaticSamplers)
	{
		memcpy(pStaticSamplers, rootSignatureDesc.pStaticSamplers,
		       sizeof(D3D12_STATIC_SAMPLER_DESC) * numStaticSamplers);
	}

	RootSignatureDesc.NumStaticSamplers = numStaticSamplers;
	RootSignatureDesc.pStaticSamplers = pStaticSamplers;

	const D3D12_ROOT_SIGNATURE_FLAGS flags = rootSignatureDesc.Flags;
	RootSignatureDesc.Flags = flags;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc;
	versionRootSignatureDesc.Init_1_1(numParameters, pParameters, numStaticSamplers, pStaticSamplers, flags);

	// Serialize the root signature.
	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	auto hResult = D3DX12SerializeVersionedRootSignature(&versionRootSignatureDesc,
		rootSignatureVersion, &rootSignatureBlob, &errorBlob);
	if (FAILED(hResult))
	{
		char* error = (char *) errorBlob->GetBufferPointer();
		ThrowIfFailed(hResult);
	}
	

	// Create the root signature.
	ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
	                                          rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&D3d12RootSignature)));
}

uint32_t RootSignature::GetDescriptorTableBitMask(const D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const
{
	uint32_t descriptorTableBitMask = 0;
	switch (descriptorHeapType)
	{
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
		descriptorTableBitMask = DescriptorTableBitMask;
		break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
		descriptorTableBitMask = SamplerTableBitMask;
		break;
	default:
		break;
	}

	return descriptorTableBitMask;
}

uint32_t RootSignature::GetNumDescriptors(const uint32_t rootIndex) const
{
	assert(rootIndex < MAX_DESCRIPTOR_TABLES);
	return NumDescriptorsPerTable[rootIndex];
}
