#include "pgcpch.h"
#include <PlagC/Editor/PickingMath.h>

#include <cmath>
#include <stdexcept>

namespace PlagC::Editor {
namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr float Epsilon = 1.0e-8f;

bool Finite(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vec3 Add(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Mul(Vec3 value, float scalar) {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float Dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float LengthSquared(Vec3 value) {
    return Dot(value, value);
}

Vec3 Normalize(Vec3 value) {
    const float lengthSquared = LengthSquared(value);
    if (!std::isfinite(lengthSquared) || lengthSquared <= Epsilon)
        throw std::invalid_argument("pick ray has a degenerate direction");
    return Mul(value, 1.0f / std::sqrt(lengthSquared));
}

} // namespace

PickRay BuildPickRay(const PickRequest& request) {
    if (!request.width || !request.height)
        throw std::invalid_argument("pick viewport extent must be non-zero");
    if (!std::isfinite(request.x) || !std::isfinite(request.y) ||
        request.x < 0.0f || request.y < 0.0f ||
        request.x > static_cast<float>(request.width) ||
        request.y > static_cast<float>(request.height))
        throw std::invalid_argument("pick coordinates are outside the viewport");

    const CameraDesc& camera = request.camera;
    if (!Finite(camera.position) || !Finite(camera.target) ||
        !std::isfinite(camera.fov) || camera.fov <= 1.0f || camera.fov >= 179.0f ||
        !std::isfinite(camera.nearClip) || !std::isfinite(camera.farClip) ||
        camera.nearClip <= 0.0f || camera.farClip <= camera.nearClip ||
        !std::isfinite(camera.aspect) || camera.aspect < 0.0f)
        throw std::invalid_argument("invalid camera for picking");

    const Vec3 forward = Normalize(Sub(camera.target, camera.position));
    Vec3 referenceUp{0.0f, 1.0f, 0.0f};
    if (std::fabs(Dot(forward, referenceUp)) > 0.999f)
        referenceUp = {0.0f, 0.0f, 1.0f};

    const Vec3 right = Normalize(Cross(forward, referenceUp));
    const Vec3 up = Normalize(Cross(right, forward));

    const float ndcX = 2.0f * (request.x / static_cast<float>(request.width)) - 1.0f;
    const float ndcY = 1.0f - 2.0f * (request.y / static_cast<float>(request.height));
    const float aspect = camera.aspect > 0.0f
        ? camera.aspect
        : static_cast<float>(request.width) / static_cast<float>(request.height);
    const float tanHalfFov = std::tan(camera.fov * Pi / 360.0f);

    Vec3 direction = forward;
    direction = Add(direction, Mul(right, ndcX * tanHalfFov * aspect));
    direction = Add(direction, Mul(up, ndcY * tanHalfFov));
    direction = Normalize(direction);

    const float span = camera.farClip - camera.nearClip;
    return {
        Add(camera.position, Mul(direction, camera.nearClip)),
        Mul(direction, span),
    };
}

} // namespace PlagC::Editor
