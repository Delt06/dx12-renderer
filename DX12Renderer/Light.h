#pragma once
#include <DirectXMath.h>

struct DirectionalLight
{
	DirectionalLight()
		: m_DirectionWs(1.0f, 0.0f, 1.0f, 0.0f)
		  , m_DirectionVs(0.0f, 0.0f, 1.0f, 0.0f)
		  , m_Color(1.0f, 1.0f, 1.0f, 1.0f)
	{
	}

	DirectX::XMFLOAT4 m_DirectionWs; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 m_DirectionVs; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 m_Color;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 3 = 48 bytes 
};

struct PointLight
{
	PointLight()
		: m_PositionWs(0.0f, 0.0f, 0.0f, 1.0f)
		  , m_PositionVs(0.0f, 0.0f, 0.0f, 1.0f)
		  , m_Color(0.5f, 0.5f, 0.5f, 1.0f)
		  , m_ConstantAttenuation(1.0f)
		  , m_LinearAttenuation(0.0f)
		  , m_QuadraticAttenuation(0.0f)
	{
	}

	explicit PointLight(const DirectX::XMFLOAT4 positionWs)
		: m_PositionWs(positionWs)
		  , m_PositionVs(0.0f, 0.0f, 0.0f, 1.0f)
		, m_Color(0.5f, 0, 0, 1.0f)
		  , m_ConstantAttenuation(1.0f)
		  , m_LinearAttenuation(0.35f)
		  , m_QuadraticAttenuation(0.44f)
	{
	}

	DirectX::XMFLOAT4 m_PositionWs; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 m_PositionVs; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 m_Color;
	//----------------------------------- (16 byte boundary)
	float m_ConstantAttenuation;
	float m_LinearAttenuation;
	float m_QuadraticAttenuation;
	// Add some padding to align to 16 bytes.
	float m_Padding{};
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 4 = 64 bytes
};
