#include "Armature.h"

using namespace DirectX;

void Armature::SetBones(const std::vector<Bone>& bones)
{
    m_Bones.resize(bones.size());
    std::copy(bones.begin(), bones.end(), m_Bones.begin());

    for (size_t i = 0; i < m_Bones.size(); ++i)
    {
        const auto& bone = m_Bones[i];
        m_BoneIndicesByNames[bone.Name] = i;
    }

    m_BoneChildrenByIndex.resize(bones.size());
}

void Armature::SetBoneChildren(const size_t boneIndex, const std::vector<size_t>& childrenIndices)
{
    auto& children = m_BoneChildrenByIndex[boneIndex];
    children.resize(childrenIndices.size());
    std::copy(childrenIndices.begin(), childrenIndices.end(), children.begin());
}

const std::vector<size_t>& Armature::GetBoneChildren(size_t boneIndex) const
{
    return m_BoneChildrenByIndex[boneIndex];
}

bool Armature::HasBones() const
{
    return m_Bones.size() > 0;
}

const std::vector<Bone>& Armature::GetBones() const
{
    return m_Bones;
}
const Bone& Armature::GetBone(const std::string& name) const
{
    return m_Bones[GetBoneIndex(name)];
}

Bone& Armature::GetBone(const std::string& name)
{
    return m_Bones[GetBoneIndex(name)];
}

const Bone& Armature::GetBone(const size_t index) const
{
    return m_Bones[index];
}

Bone& Armature::GetBone(const size_t index)
{
    return m_Bones[index];
}

bool Armature::HasBone(const std::string& name) const
{
    return m_BoneIndicesByNames.contains(name);
}

size_t Armature::GetBoneIndex(const std::string& name) const
{
    if (const auto findIter = m_BoneIndicesByNames.find(name); findIter != m_BoneIndicesByNames.end())
    {
        return findIter->second;
    }

    throw std::exception("Bone not found.");
}

void Armature::UpdateBoneGlobalTransforms()
{
    for (size_t i = 0; i < m_Bones.size(); ++i)
    {
        UpdateBoneGlobalTransforms(i, XMMatrixIdentity());
    }
}

void Armature::MarkBonesDirty()
{
    for (auto& bone : m_Bones)
    {
        bone.IsDirty = true;
    }
}

void Armature::UpdateBoneGlobalTransforms(const size_t rootIndex, const XMMATRIX& parentTransform)
{
    auto& bone = m_Bones[rootIndex];
    if (!bone.IsDirty)
    {
        return;
    }

    bone.GlobalTransform = bone.LocalTransform * parentTransform;
    bone.IsDirty = false;

    for (size_t childIndex : m_BoneChildrenByIndex[rootIndex])
    {
        UpdateBoneGlobalTransforms(childIndex, bone.GlobalTransform);
    }
}
