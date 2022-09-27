#include <Framework/Animation.h>
#include <Framework/Mesh.h>
#include <Framework/Bone.h>
#include <DirectXMath.h>

#include <set>

using namespace DirectX;

namespace
{
	std::pair<Animation::KeyFrame, Animation::KeyFrame> GetInterpolatedKeyFrame(const std::vector<Animation::KeyFrame>& keyFrames, float normalizedTime)
	{
		if (keyFrames.size() == 1)
		{
			return std::make_pair(keyFrames[0], keyFrames[0]);
		}

		for (size_t i = 0; i < keyFrames.size() - 1; i++)
		{
			if (normalizedTime < (float)keyFrames[i + 1].NormalizedTime)
			{
				return std::make_pair(keyFrames[i], keyFrames[i + 1]);
			}
		}

		return std::make_pair(keyFrames[keyFrames.size() + 1], keyFrames[0]);
	}

	float InverseLerp(float a, float b, float v)
	{
		if (a == b)
		{
			return 0.0f;
		}

		return (v - a) / (b - a);
	}

	XMVECTOR Interpolate(Animation::KeyFrame first, Animation::KeyFrame second, float normalizedTime)
	{
		float t = InverseLerp(first.NormalizedTime, second.NormalizedTime, normalizedTime);
		return XMVectorLerp(first.Value, second.Value, t);
	}

	XMVECTOR InterpolateRotation(Animation::KeyFrame first, Animation::KeyFrame second, float normalizedTime)
	{
		float t = InverseLerp(first.NormalizedTime, second.NormalizedTime, normalizedTime);
		return XMQuaternionSlerp(first.Value, second.Value, t);
	}
}

Animation::Animation(float duration, float ticksPerSecond, const std::vector<Channel>& channels)
	: m_Duration(duration)
	, m_TicksPerSecond(ticksPerSecond != 0 ? ticksPerSecond : 25.0f)
	, m_Channels(channels)
{

}

std::vector<Animation::BoneTransform> Animation::GetBonesTranforms(Mesh& mesh, double time) const
{
	if (!mesh.HasBones())
		throw std::exception("Can't play an animation on a mesh without bones.");

	double timeInTicks = time * m_TicksPerSecond;
	float normalizedTime = static_cast<float>(fmod(timeInTicks, m_Duration));

	std::vector<BoneTransform> tranforms;
	tranforms.resize(mesh.GetBones().size());

	std::set<size_t> affectedBones;

	for (const auto& channel : m_Channels)
	{
		if (!mesh.HasBone(channel.NodeName))
		{
			continue;
		}

		auto& bone = mesh.GetBone(channel.NodeName);
		size_t boneIndex = mesh.GetBoneIndex(channel.NodeName);
		affectedBones.insert(boneIndex);

		const auto positionKfs = GetInterpolatedKeyFrame(channel.PositionKeyFrames, normalizedTime);
		const auto position = Interpolate(positionKfs.first, positionKfs.second, normalizedTime);

		const auto rotationKfs = GetInterpolatedKeyFrame(channel.RotationKeyFrames, normalizedTime);
		const auto rotation = InterpolateRotation(rotationKfs.first, rotationKfs.second, normalizedTime);

		const auto scalingKfs = GetInterpolatedKeyFrame(channel.ScalingKeyFrames, normalizedTime);
		const auto scaling = Interpolate(scalingKfs.first, scalingKfs.second, normalizedTime);

		tranforms[boneIndex] = { position, rotation, scaling };
	}

	for (size_t i = 0; i < mesh.GetBones().size(); ++i)
	{
		if (affectedBones.contains(i))
		{
			continue;
		}

		tranforms[i] = { XMVECTOR(), XMQuaternionIdentity(), XMVectorSet(1, 1, 1, 0) };
	}

	return tranforms;
}

void Animation::Apply(Mesh& mesh, const std::vector<BoneTransform>& transforms)
{
	if (mesh.GetBones().size() != transforms.size())
	{
		throw std::exception("Sizes are different.");
	}

	for (size_t i = 0; i < transforms.size(); ++i)
	{
		const auto& transform = transforms[i];
		auto translationMatrix = XMMatrixTranslationFromVector(transform.Position);
		auto rotationMatrix = XMMatrixRotationQuaternion(transform.Rotation);
		auto scalingMatrix = XMMatrixScalingFromVector(transform.Scaling);

		mesh.GetBone(i).LocalTransform = scalingMatrix * rotationMatrix * translationMatrix;
	}

	mesh.MarkBonesDirty();
}

Animation::BoneMask Animation::BuildMask(const Mesh& mesh, const std::string& rootBoneName)
{
	BoneMask mask;
	const auto rootIndex = mesh.GetBoneIndex(rootBoneName);
	BuildMaskRecursive(mesh, rootIndex, mask);
	return mask;
}

std::vector<Animation::BoneTransform> Animation::Blend(const std::vector<BoneTransform>& transforms1, const std::vector<BoneTransform>& transforms2, float weight)
{
	if (transforms1.size() != transforms2.size())
	{
		throw std::exception("Sizes are different.");
	}

	std::vector<Animation::BoneTransform> result;
	result.resize(transforms1.size());

	for (size_t i = 0; i < transforms1.size(); ++i)
	{
		const auto& t1 = transforms1[i];
		const auto& t2 = transforms2[i];

		auto& tr = result[i];
		tr.Position = XMVectorLerp(t1.Position, t2.Position, weight);
		tr.Rotation = XMQuaternionSlerp(t1.Rotation, t2.Rotation, weight);
		tr.Scaling = XMVectorLerp(t1.Scaling, t2.Scaling, weight);
	}

	return result;
}

std::vector<Animation::BoneTransform> Animation::ApplyMask(const std::vector<BoneTransform>& transforms1, const std::vector<BoneTransform>& transforms2, const BoneMask& boneMask)
{
	if (transforms1.size() != transforms2.size())
	{
		throw std::exception("Sizes are different.");
	}

	std::vector<Animation::BoneTransform> result;
	result.resize(transforms1.size());

	for (size_t i = 0; i < transforms1.size(); ++i)
	{
		const auto& t1 = transforms1[i];
		const auto& t2 = transforms2[i];

		result[i] = boneMask.contains(i) ? t1 : t2;
	}

	return result;
}

void Animation::BuildMaskRecursive(const Mesh& mesh, size_t rootBoneIndex, BoneMask& result)
{
	if (result.contains(rootBoneIndex))
	{
		return;
	}

	result.insert(rootBoneIndex);
	const auto& children = mesh.GetBoneChildren(rootBoneIndex);

	for (const auto childIndex : children)
	{
		BuildMaskRecursive(mesh, childIndex, result);
	}
}
