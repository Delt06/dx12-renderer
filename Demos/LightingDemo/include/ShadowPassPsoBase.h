#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <memory>
#include <DirectXMath.h>
#include <Framework/Material.h>
#include <Framework/ShaderResourceView.h>

class GameObject;
class Camera;
class CommandList;

class ShadowPassPsoBase
{
public:
    explicit ShadowPassPsoBase(const std::shared_ptr<CommonRootSignature>& rootSignature, UINT resolution = 4096);
    virtual ~ShadowPassPsoBase();

    void Begin(CommandList& commandList) const;
    void End(CommandList& commandList) const;

    void SetBias(float depthBias, float normalBias);
    void DrawToShadowMap(CommandList& commandList, const GameObject& gameObject) const;
    [[nodiscard]] DirectX::XMMATRIX GetShadowViewProjectionMatrix() const;

    virtual void SetRenderTarget(CommandList& commandList) const = 0;
    virtual void ClearShadowMap(CommandList& commandList) const = 0;
    virtual ShaderResourceView GetShadowMapShaderResourceView() const = 0;

protected:
    static constexpr DXGI_FORMAT SHADOW_MAP_FORMAT = DXGI_FORMAT_D32_FLOAT;

    struct ShadowPassParameters
    {
        DirectX::XMMATRIX Model;
        DirectX::XMMATRIX InverseTransposeModel;
        DirectX::XMMATRIX ViewProjection;
        DirectX::XMFLOAT4 LightDirectionWs;
        DirectX::XMFLOAT4 Bias;
        uint32_t LightType;
        float Padding[3];

        enum : uint32_t
        {
            DirectionalLight = 0,
            PointLight,
            SpotLight,
        };
    };

    ShadowPassParameters m_ShadowPassParameters;
    const UINT m_Resolution;

private:
    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;
    std::shared_ptr<CommonRootSignature> m_RootSignature;
    std::shared_ptr<Material> m_Material;
};
