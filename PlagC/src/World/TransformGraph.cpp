#include "pgcpch.h"
#include <PlagC/World/TransformGraph.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace PlagC {
namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr float Degrees = 180.0f / Pi;
constexpr float Radians = Pi / 180.0f;
constexpr float Epsilon = 1.0e-8f;

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

bool Finite(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void Validate(const Transform& transform) {
    if (!Finite(transform.position) || !Finite(transform.rotationDegrees) ||
        !Finite(transform.scale) || transform.scale.x <= 0.0f ||
        transform.scale.y <= 0.0f || transform.scale.z <= 0.0f)
        throw std::invalid_argument("invalid hierarchical transform");
}

Quat Mul(Quat a, Quat b) {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

Quat AxisAngle(Vec3 axis, float radians) {
    const float half = radians * 0.5f;
    const float s = std::sin(half);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(half)};
}

Quat Rotation(Vec3 eulerDegrees) {
    const auto yaw = AxisAngle({0.0f, 1.0f, 0.0f}, eulerDegrees.y * Radians);
    const auto pitch = AxisAngle({1.0f, 0.0f, 0.0f}, eulerDegrees.x * Radians);
    const auto roll = AxisAngle({0.0f, 0.0f, 1.0f}, eulerDegrees.z * Radians);
    return Mul(Mul(yaw, pitch), roll);
}

Quat Conjugate(Quat q) {
    return {-q.x, -q.y, -q.z, q.w};
}

float LengthSquared(Quat q) {
    return q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
}

Quat Normalize(Quat q) {
    const float lengthSquared = LengthSquared(q);
    if (!std::isfinite(lengthSquared) || lengthSquared <= Epsilon)
        throw std::invalid_argument("invalid hierarchical rotation");
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {q.x * inverseLength, q.y * inverseLength, q.z * inverseLength,
            q.w * inverseLength};
}

Quat Inverse(Quat q) {
    q = Normalize(q);
    return Conjugate(q);
}

Vec3 Rotate(Quat q, Vec3 value) {
    q = Normalize(q);
    const Quat vector{value.x, value.y, value.z, 0.0f};
    const Quat rotated = Mul(Mul(q, vector), Conjugate(q));
    return {rotated.x, rotated.y, rotated.z};
}

Vec3 Euler(Quat q) {
    q = Normalize(q);
    const Vec3 x = Rotate(q, {1.0f, 0.0f, 0.0f});
    const Vec3 y = Rotate(q, {0.0f, 1.0f, 0.0f});
    const Vec3 z = Rotate(q, {0.0f, 0.0f, 1.0f});
    const float pitch = std::asin(std::clamp(-z.y, -1.0f, 1.0f));
    if (std::abs(std::cos(pitch)) < 1.0e-5f)
        return {pitch * Degrees, std::atan2(-x.z, x.x) * Degrees, 0.0f};
    return {
        pitch * Degrees,
        std::atan2(z.x, z.z) * Degrees,
        std::atan2(x.y, y.y) * Degrees,
    };
}

Vec3 Add(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Mul(Vec3 a, Vec3 b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

Vec3 Div(Vec3 a, Vec3 b) {
    return {a.x / b.x, a.y / b.y, a.z / b.z};
}

} // namespace

Transform ComposeTransform(const Transform& parentWorld, const Transform& local) {
    Validate(parentWorld);
    Validate(local);

    const Quat parentRotation = Rotation(parentWorld.rotationDegrees);
    const Quat localRotation = Rotation(local.rotationDegrees);
    const Vec3 scaledLocalPosition = Mul(parentWorld.scale, local.position);

    Transform result;
    result.position = Add(parentWorld.position, Rotate(parentRotation, scaledLocalPosition));
    result.rotationDegrees = Euler(Mul(parentRotation, localRotation));
    result.scale = Mul(parentWorld.scale, local.scale);
    Validate(result);
    return result;
}

Transform RelativeTransform(const Transform& parentWorld, const Transform& world) {
    Validate(parentWorld);
    Validate(world);

    const Quat parentRotation = Rotation(parentWorld.rotationDegrees);
    const Quat worldRotation = Rotation(world.rotationDegrees);

    Transform result;
    result.position = Div(Rotate(Inverse(parentRotation), Sub(world.position, parentWorld.position)),
                          parentWorld.scale);
    result.rotationDegrees = Euler(Mul(Inverse(parentRotation), worldRotation));
    result.scale = Div(world.scale, parentWorld.scale);
    Validate(result);
    return result;
}

Vec3 RotateDirection(Vec3 rotationDegrees, Vec3 direction) {
    if (!Finite(rotationDegrees) || !Finite(direction))
        throw std::invalid_argument("invalid direction rotation");
    return Rotate(Rotation(rotationDegrees), direction);
}

Vec3 DirectionRotation(Vec3 direction) {
    const float length = std::sqrt(direction.x*direction.x + direction.y*direction.y + direction.z*direction.z);
    if (!Finite(direction) || !std::isfinite(length) || length <= Epsilon)
        throw std::invalid_argument("look direction must be finite and nonzero");
    return {std::asin(std::clamp(direction.y/length, -1.0f, 1.0f))*Degrees,
            std::atan2(-direction.x, -direction.z)*Degrees, 0.0f};
}

Vec3 DirectionRotation(Vec3 direction, Vec3 up) {
    auto angles = DirectionRotation(direction);
    if (!Finite(up)) throw std::invalid_argument("invalid camera up");
    const auto right = RotateDirection(angles, {1,0,0});
    const auto baseUp = RotateDirection(angles, {0,1,0});
    const float x = up.x*right.x + up.y*right.y + up.z*right.z;
    const float y = up.x*baseUp.x + up.y*baseUp.y + up.z*baseUp.z;
    if (x*x+y*y <= Epsilon) throw std::invalid_argument("camera up is parallel to forward");
    angles.z = std::atan2(-x,y)*Degrees;
    return angles;
}

} // namespace PlagC
