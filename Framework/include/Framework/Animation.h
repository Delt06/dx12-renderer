#pragma once

#include <DirectXMath.h>

#include <string>
#include <vector>
#include <memory>
#include <set>

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

	using BoneMask = std::set<size_t>;

	std::vector<BoneTransform> GetBonesTransforms(Mesh& mesh, double time) const;

	static void Apply(Mesh& mesh, const std::vector<BoneTransform>& transforms);

	static BoneMask BuildMask(const Mesh& mesh, const std::string& rootBoneName);
	static std::vector<BoneTransform> Blend(const std::vector<BoneTransform>& transforms1, const std::vector<BoneTransform>& transforms2, float weight);
	static std::vector<BoneTransform> ApplyMask(const std::vector<BoneTransform>& transforms1, const std::vector<BoneTransform>& transforms2, const BoneMask& boneMask);

private:

	static void BuildMaskRecursive(const Mesh& mesh, size_t rootBoneIndex, BoneMask& result);

	float m_Duration;
	float m_TicksPerSecond;
	std::vector<Channel> m_Channels;
};