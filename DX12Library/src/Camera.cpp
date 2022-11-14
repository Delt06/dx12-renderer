#include "Camera.h"

using namespace DirectX;

Camera::Camera()
    : VFov(45.0f)
    , AspectRatio(1.0f)
    , ZNear(0.1f)
    , ZFar(100.0f)
    , ViewDirty(true)
    , InverseViewDirty(true)
    , ProjectionDirty(true)
    , InverseProjectionDirty(true)
{
    PData = static_cast<AlignedData*>(_aligned_malloc(sizeof(AlignedData), ALIGNMENT));
    PData->Translation = XMVectorZero();
    PData->QRotation = XMQuaternionIdentity();
}

Camera::~Camera()
{
    _aligned_free(PData);
}

void Camera::SetLookAt(FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up)
{
    PData->ViewMatrix = XMMatrixLookAtLH(eye, target, up);

    PData->Translation = eye;
    PData->QRotation = XMQuaternionRotationMatrix(XMMatrixTranspose(PData->ViewMatrix));

    InverseViewDirty = true;
    ViewDirty = false;
}

XMMATRIX Camera::GetViewMatrix() const
{
    if (ViewDirty)
    {
        UpdateViewMatrix();
    }
    return PData->ViewMatrix;
}

XMMATRIX Camera::GetInverseViewMatrix() const
{
    if (InverseViewDirty)
    {
        PData->InverseViewMatrix = XMMatrixInverse(nullptr, PData->ViewMatrix);
        InverseViewDirty = false;
    }

    return PData->InverseViewMatrix;
}

void Camera::SetProjection(const float vFov, const float aspectRatio, const float zNear, const float zFar)
{
    VFov = vFov;
    AspectRatio = aspectRatio;
    ZNear = zNear;
    ZFar = zFar;

    ProjectionDirty = true;
    InverseProjectionDirty = true;
}

XMMATRIX Camera::GetProjectionMatrix() const
{
    if (ProjectionDirty)
    {
        UpdateProjectionMatrix();
    }

    return PData->ProjectionMatrix;
}

XMMATRIX Camera::GetInverseProjectionMatrix() const
{
    if (InverseProjectionDirty)
    {
        UpdateInverseProjectionMatrix();
    }

    return PData->InverseProjectionMatrix;
}

void Camera::SetFov(const float vFov)
{
    if (VFov == vFov) return;

    VFov = vFov;
    ProjectionDirty = true;
    InverseProjectionDirty = true;
}

float Camera::GetFov() const
{
    return VFov;
}

void Camera::SetTranslation(FXMVECTOR translation)
{
    PData->Translation = translation;
    ViewDirty = true;
    InverseViewDirty = true;
}

XMVECTOR Camera::GetTranslation() const
{
    return PData->Translation;
}

void Camera::SetRotation(FXMVECTOR qRotation)
{
    PData->QRotation = qRotation;
}

XMVECTOR Camera::GetRotation() const
{
    return PData->QRotation;
}

void Camera::Translate(FXMVECTOR translation, const Space space)
{
    switch (space)
    {
    case Space::Local:
        {
            PData->Translation += XMVector3Rotate(translation, PData->QRotation);
        }
        break;
    case Space::World:
        {
            PData->Translation += translation;
        }
        break;
    default:;
    }

    PData->Translation = XMVectorSetW(PData->Translation, 1.0f);

    ViewDirty = true;
    InverseViewDirty = true;
}

void Camera::Rotate(FXMVECTOR qRotation)
{
    PData->QRotation = XMQuaternionMultiply(PData->QRotation, qRotation);

    ViewDirty = true;
    InverseViewDirty = true;
}

Camera::Frustum Camera::GetFrustum() const
{
    Frustum frustum;

    // http://davidlively.com/programming/graphics/frustum-calculation-and-culling-hopefully-demystified/

    const auto toWorld = GetInverseViewMatrix();

    frustum.m_Planes[Frustum::NearPlane] = FrustumPlane(XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 0, ZNear, 1), toWorld);
    frustum.m_Planes[Frustum::FarPlane] = FrustumPlane(XMVectorSet(0, 0, -1, 0), XMVectorSet(0, 0, ZFar, 1), toWorld);

    const float hh = tanf(XMConvertToRadians(VFov) * 0.5f);
    const float hw = hh * AspectRatio;

    const auto nw = XMVector3Normalize(XMVectorSet(-hw, hh, 1, 0));
    const auto ne = XMVector3Normalize(XMVectorSet(hw, hh, 1, 0));
    const auto se = XMVector3Normalize(XMVectorSet(hw, -hh, 1, 0));
    const auto sw = XMVector3Normalize(XMVectorSet(-hw, -hh, 1, 0));

    const auto origin = XMVectorSet(0, 0, 0, 1);

    frustum.m_Planes[Frustum::TopPlane] = FrustumPlane(-XMVector3Cross(nw, ne), origin, toWorld);
    frustum.m_Planes[Frustum::RightPlane] = FrustumPlane(-XMVector3Cross(ne, se), origin, toWorld);
    frustum.m_Planes[Frustum::BottomPlane] = FrustumPlane(-XMVector3Cross(se, sw), origin, toWorld);
    frustum.m_Planes[Frustum::LeftPlane] = FrustumPlane(-XMVector3Cross(sw, nw), origin, toWorld);

    return frustum;
}

void Camera::UpdateViewMatrix() const
{
    const XMMATRIX rotationMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(PData->QRotation));
    const XMMATRIX translationMatrix = XMMatrixTranslationFromVector((-PData->Translation));

    PData->ViewMatrix = translationMatrix * rotationMatrix;

    InverseViewDirty = true;
    ViewDirty = false;
}

void Camera::UpdateInverseViewMatrix() const
{
    if (ViewDirty)
    {
        UpdateViewMatrix();
    }

    PData->InverseViewMatrix = XMMatrixInverse(nullptr, PData->ViewMatrix);
    InverseViewDirty = false;
}

void Camera::UpdateProjectionMatrix() const
{
    PData->ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(VFov), AspectRatio, ZNear, ZFar);

    ProjectionDirty = false;
    InverseProjectionDirty = true;
}

void Camera::UpdateInverseProjectionMatrix() const
{
    if (ProjectionDirty)
    {
        UpdateProjectionMatrix();
    }

    PData->InverseProjectionMatrix = XMMatrixInverse(nullptr, PData->ProjectionMatrix);
    InverseProjectionDirty = false;
}

Camera::FrustumPlane::FrustumPlane(DirectX::XMVECTOR normal, DirectX::XMVECTOR point)
{
    normal = DirectX::XMVector3Normalize(normal);
    DirectX::XMStoreFloat3(&m_Normal, normal);
    DirectX::XMStoreFloat(&m_Distance, DirectX::XMVector3Dot(normal, point));
}

Camera::FrustumPlane::FrustumPlane(DirectX::XMVECTOR normal, DirectX::XMVECTOR point, DirectX::XMMATRIX view)
{
    normal = DirectX::XMVector3Normalize(DirectX::XMVector4Transform(normal, view));
    point = DirectX::XMVector4Transform(point, view);
    DirectX::XMStoreFloat3(&m_Normal, normal);
    DirectX::XMStoreFloat(&m_Distance, DirectX::XMVector3Dot(normal, point));
}

Camera::FrustumPlane::FrustumPlane()
    : m_Normal{}
    , m_Distance(0)
{

}
