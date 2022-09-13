#pragma once

#include <DirectXMath.h>

#include <string>
#include <vector>
#include <memory>

class Mesh;

class Animation
{
public:
	struct KeyFrame
	{
		DirectX::XMVECTOR Value;
		float NormalizedTime;
	};

	struct Channel
	{
		std::string NodeName;
		std::vector<KeyFrame> PositionKeyFrames;
		std::vector<KeyFrame> RotationKeyFrames;
		std::vector<KeyFrame> ScalingKeyFrames;
	};

	explicit Animation(float duration, float ticksPerSecond, const std::vector<Channel>& channels);

	void Play(Mesh& mesh, double time);

private:
	float m_Duration;
	float m_TicksPerSecond;
	std::vector<Channel> m_Channels;
};