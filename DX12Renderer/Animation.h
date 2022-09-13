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

	struct BoneTransform
	{
		DirectX::XMVECTOR Position;
		DirectX::XMVECTOR Rotation;
		DirectX::XMVECTOR Scaling;
	};

	std::vector<BoneTransform> GetBonesTranforms(Mesh& mesh, double time) const;

	static void Apply(Mesh& mesh, const std::vector<BoneTransform>& transforms);

	static std::vector<BoneTransform> Blend(const std::vector<BoneTransform>& transforms1, const std::vector<BoneTransform>& transforms2, float weight);

private:
	float m_Duration;
	float m_TicksPerSecond;
	std::vector<Channel> m_Channels;
};