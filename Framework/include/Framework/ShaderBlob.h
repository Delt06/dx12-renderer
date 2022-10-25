#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <string>

class ShaderBlob
{
public:
	ShaderBlob(const std::wstring& fileName);

	ShaderBlob(const void* bytecode, size_t length);

	const Microsoft::WRL::ComPtr<ID3DBlob>& GetBlob() const;

private:
	Microsoft::WRL::ComPtr<ID3DBlob> m_Blob;
};