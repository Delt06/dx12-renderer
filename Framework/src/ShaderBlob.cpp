#include "ShaderBlob.h"

#include <d3dcompiler.h>
#include <DX12Library/ShaderUtils.h>
#include <DX12Library/Helpers.h>

ShaderBlob::ShaderBlob(const void* bytecode, size_t length)
{
	ThrowIfFailed(D3DCreateBlob(length, &m_Blob));
	memcpy(m_Blob->GetBufferPointer(), bytecode, length);
}

ShaderBlob::ShaderBlob(const std::wstring& fileName)
	: m_Blob(ShaderUtils::LoadShaderFromFile(fileName))
{

}

const Microsoft::WRL::ComPtr<ID3DBlob>& ShaderBlob::GetBlob() const
{
	return m_Blob;
}
