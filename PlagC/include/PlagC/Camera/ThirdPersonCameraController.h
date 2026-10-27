#pragma once
#include <PlagC/Camera/CameraSystem.h>
#include <PlagC/Physics/PhysicsSystem.h>
#include <PlagC/LatencyProfiler.h>
namespace PlagC
{
class ThirdPersonCameraController
{
    float arm_ = 4.2f, fov_ = 65;
  public:
    CameraLook look{-90, -12, .12f};
    CameraDesc Update(PhysicsSystem &physics, Vec3 pivot, bool aim, double dt)
    {
        if (!std::isfinite(dt) || dt < 0) throw std::invalid_argument("invalid camera timestep");
        look.pitch = std::clamp(look.pitch, -65.f, 65.f);
        auto f = look.Forward(); constexpr float r = 3.14159265f / 180;
        Vec3 right{-std::sin(look.yaw * r), 0, std::cos(look.yaw * r)};
        const float distance = aim ? 2.1f : 4.2f;
        Vec3 offset{right.x * .55f - f.x * distance, -f.y * distance, right.z * .55f - f.z * distance};
        ColliderComponent sphere; sphere.type = ColliderType::Sphere; sphere.dimensions = {.22f, .22f, .22f};
        auto hit = physics.ShapeCast(sphere, {pivot, {}, {1, 1, 1}}, offset);
        float fraction = hit ? std::max(0.f, hit->fraction - .03f) : 1.f;
        float allowed = distance * fraction;
        arm_ = allowed < arm_ ? allowed : std::min(allowed, arm_ + float(dt) * 5);
        float scale = arm_ / distance;
        Vec3 p{pivot.x + offset.x * scale, pivot.y + offset.y * scale, pivot.z + offset.z * scale};
        fov_ += ((aim ? 48.f : 65.f) - fov_) * (1 - std::exp(-float(dt) * 12));
        LatencyProfiler::OnCamera();
        return {p, {p.x + f.x, p.y + f.y, p.z + f.z}, fov_, .05f, 600};
    }
};
} // namespace PlagC
