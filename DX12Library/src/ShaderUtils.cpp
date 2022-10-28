#include "ShaderUtils.h"
#include <dxcapi.h>
#include "Helpers.h"
#include "Application.h"

namespace hlsl
{
    // https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/DxilContainer/DxilContainer.h
#define DXIL_FOURCC(ch0, ch1, ch2, ch3) (                            \
        (uint32_t)(uint8_t)(ch0)        | (uint32_t)(uint8_t)(ch1) << 8  | \
        (uint32_t)(uint8_t)(ch2) << 16  | (uint32_t)(uint8_t)(ch3) << 24   \
        )

    enum DxilFourCC
    {
        DFCC_Container = DXIL_FOURCC('D', 'X', 'B', 'C'), // for back-compat with tools that look for DXBC containers
        DFCC_ResourceDef = DXIL_FOURCC('R', 'D', 'E', 'F'),
        DFCC_InputSignature = DXIL_FOURCC('I', 'S', 'G', '1'),
        DFCC_OutputSignature = DXIL_FOURCC('O', 'S', 'G', '1'),
        DFCC_PatchConstantSignature = DXIL_FOURCC('P', 'S', 'G', '1'),
        DFCC_ShaderStatistics = DXIL_FOURCC('S', 'T', 'A', 'T'),
        DFCC_ShaderDebugInfoDXIL = DXIL_FOURCC('I', 'L', 'D', 'B'),
        DFCC_ShaderDebugName = DXIL_FOURCC('I', 'L', 'D', 'N'),
        DFCC_FeatureInfo = DXIL_FOURCC('S', 'F', 'I', '0'),
        DFCC_PrivateData = DXIL_FOURCC('P', 'R', 'I', 'V'),
        DFCC_RootSignature = DXIL_FOURCC('R', 'T', 'S', '0'),
        DFCC_DXIL = DXIL_FOURCC('D', 'X', 'I', 'L'),
        DFCC_PipelineStateValidation = DXIL_FOURCC('P', 'S', 'V', '0'),
        DFCC_RuntimeData = DXIL_FOURCC('R', 'D', 'A', 'T'),
        DFCC_ShaderHash = DXIL_FOURCC('H', 'A', 'S', 'H'),
        DFCC_ShaderSourceInfo = DXIL_FOURCC('S', 'R', 'C', 'I'),
        DFCC_ShaderPDBInfo = DXIL_FOURCC('P', 'D', 'B', 'I'),
        DFCC_CompilerVersion = DXIL_FOURCC('V', 'E', 'R', 'S'),
    };

#undef DXIL_FOURCC
}



Microsoft::WRL::ComPtr<ID3DBlob> ShaderUtils::LoadShaderFromFile(const std::wstring& fileName)
{
    const auto completePath = L"Shaders/" + fileName;

    const auto& library = Application::Get().GetDxcLibrary();
    uint32_t codePage = CP_UTF8;
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
    ThrowIfFailed(library->CreateBlobFromFile(completePath.c_str(), &codePage, &sourceBlob));

    Microsoft::WRL::ComPtr<ID3DBlob> result;
    ThrowIfFailed(sourceBlob.As(&result));

    return result;
}

Microsoft::WRL::ComPtr<ID3D12ShaderReflection> ShaderUtils::Reflect(const Microsoft::WRL::ComPtr<ID3DBlob>& shaderSource)
{
    Microsoft::WRL::ComPtr<IDxcBlob> shaderSourceDxc;
    ThrowIfFailed(shaderSource.As(&shaderSourceDxc));

    Microsoft::WRL::ComPtr<IDxcContainerReflection> pReflection;
    UINT32 shaderIdx;
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&pReflection)));
    ThrowIfFailed(pReflection->Load(shaderSourceDxc.Get()));
    ThrowIfFailed(pReflection->FindFirstPartKind(hlsl::DFCC_DXIL, &shaderIdx));

    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
    ThrowIfFailed(pReflection->GetPartReflection(shaderIdx, __uuidof(ID3D12ShaderReflection), (void**)&reflection));

    return reflection;
}

std::vector<ShaderUtils::ConstantBufferMetadata> ShaderUtils::GetConstantBuffers(const Microsoft::WRL::ComPtr<ID3D12ShaderReflection>& shaderReflection)
{
    // https://github.com/planetpratik/DirectXTutorials/blob/master/SimpleShader.cpp
    D3D12_SHADER_DESC shaderDesc;
    ThrowIfFailed(shaderReflection->GetDesc(&shaderDesc));

    std::vector<ShaderUtils::ConstantBufferMetadata> result;
    result.reserve(shaderDesc.ConstantBuffers);

    for (UINT cbufferIndex = 0; cbufferIndex < shaderDesc.ConstantBuffers; ++cbufferIndex)
    {
        const auto constantBuffer = shaderReflection->GetConstantBufferByIndex(cbufferIndex);
        D3D12_SHADER_BUFFER_DESC cbufferDesc;
        ThrowIfFailed(constantBuffer->GetDesc(&cbufferDesc));

        D3D12_SHADER_INPUT_BIND_DESC inputBindDesc;
        ThrowIfFailed(shaderReflection->GetResourceBindingDescByName(cbufferDesc.Name, &inputBindDesc));

        ConstantBufferMetadata constantBufferMetadata;
        constantBufferMetadata.Name = cbufferDesc.Name;
        constantBufferMetadata.RegisterIndex = inputBindDesc.BindPoint;
        constantBufferMetadata.Space = inputBindDesc.Space;
        constantBufferMetadata.Variables.reserve(cbufferDesc.Variables);
        constantBufferMetadata.Size = 0;

        for (UINT variableIndex = 0; variableIndex < cbufferDesc.Variables; ++variableIndex)
        {
            auto variable = constantBuffer->GetVariableByIndex(variableIndex);
            D3D12_SHADER_VARIABLE_DESC variableDesc;
            ThrowIfFailed(variable->GetDesc(&variableDesc));

            ConstantBufferMetadata::Variable variableMetadata;
            variableMetadata.Name = variableDesc.Name;
            variableMetadata.Offset = variableDesc.StartOffset;
            variableMetadata.Size = variableDesc.Size;

            if (variableDesc.DefaultValue != nullptr)
            {
                variableMetadata.DefaultValue = std::shared_ptr<uint8_t[]>(new uint8_t[variableDesc.Size]);
                memcpy(variableMetadata.DefaultValue.get(), variableDesc.DefaultValue, variableDesc.Size);
            }
            else
            {
                variableDesc.DefaultValue = nullptr;
            }


            constantBufferMetadata.Size = max(constantBufferMetadata.Size, variableMetadata.Offset + variableMetadata.Size);
            constantBufferMetadata.Variables.push_back(variableMetadata);
        }

        result.push_back(constantBufferMetadata);
    }

    return result;
}

std::vector<ShaderUtils::ShaderResourceViewMetadata> ShaderUtils::GetShaderResourceViews(const Microsoft::WRL::ComPtr<ID3D12ShaderReflection>& shaderReflection)
{
    // https://github.com/planetpratik/DirectXTutorials/blob/master/SimpleShader.cpp
    D3D12_SHADER_DESC shaderDesc;
    ThrowIfFailed(shaderReflection->GetDesc(&shaderDesc));

    std::vector<ShaderUtils::ShaderResourceViewMetadata> result;
    result.reserve(shaderDesc.BoundResources);

    for (UINT resourceIndex = 0; resourceIndex < shaderDesc.BoundResources; ++resourceIndex)
    {
        D3D12_SHADER_INPUT_BIND_DESC inputBindDesc;
        ThrowIfFailed(shaderReflection->GetResourceBindingDesc(resourceIndex, &inputBindDesc));

        switch (inputBindDesc.Type)
        {
        case D3D_SIT_TEXTURE:
            {
                ShaderResourceViewMetadata resourceMetadata;
                resourceMetadata.Name = inputBindDesc.Name;
                resourceMetadata.RegisterIndex = inputBindDesc.BindPoint;
                resourceMetadata.Space = inputBindDesc.Space;
                result.push_back(resourceMetadata);
                break;
            }
        }
    }

    return result;
}
