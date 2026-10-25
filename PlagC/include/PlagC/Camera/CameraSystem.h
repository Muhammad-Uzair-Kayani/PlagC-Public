#pragma once
#include <PlagC/World/World.h>
#include <cmath>
#include <algorithm>
namespace PlagC
{
// Input-independent controllers. The application supplies mouse deltas/movement;
// physics owns FPS translation, while a free camera owns its own translation.
struct CameraLook
{
    float yaw = -90, pitch = 0, sensitivity = .12f;
    void Rotate(float dx, float dy)
    {
        if (!std::isfinite(dx) || !std::isfinite(dy))
            throw std::invalid_argument("invalid camera input");
        yaw = std::remainder(yaw + dx * sensitivity, 360.f);
        pitch = std::clamp(pitch - dy * sensitivity, -89.f, 89.f);
    }
    Vec3 Forward() const
    {
        constexpr float r = 3.14159265358979323846f / 180;
        return {std::cos(yaw * r) * std::cos(pitch * r), std::sin(pitch * r),
                std::sin(yaw * r) * std::cos(pitch * r)};
    }
};
class CameraSystem
{
    unsigned width_ = 0, height_ = 0;

  public:
    void SetViewport(unsigned width, unsigned height)
    {
        width_ = width;
        height_ = height;
    }
    void Activate(World &world, Entity camera)
    {
        world.SetActiveCamera(camera);
    }
    Entity Active(const World &world) const
    {
        return world.ActiveCamera();
    }
    CameraDesc Extract(const World &world) const
    {
        auto e = world.ActiveCamera();
        if (!e)
            throw std::logic_error("no active camera");
        auto record = world.Read(e);
        const auto &c = *record.camera;
        CameraDesc d{record.transform.position, c.target, c.fov, c.nearClip, c.farClip};
        d.aspect = c.aspect > 0 ? c.aspect : (height_ ? float(width_) / height_ : 0.f);
        return d;
    }
    bool Sync(const World &world, Renderer &renderer) const
    {
        if (!world.ActiveCamera())
            return false;
        renderer.SetCamera(Extract(world));
        return true;
    }
};
class FPSCameraController
{
  public:
    CameraLook look;
    void Update(World &world, Entity camera, Vec3 eye) const
    {
        auto c = world.Read(camera).camera;
        if (!c)
            throw std::invalid_argument("FPS controller requires camera");
        auto f = look.Forward();
        c->target = {eye.x + f.x, eye.y + f.y, eye.z + f.z};
        // SetCamera and SetTransform each validate the look-at pair. Use a target
        // away from both positions before moving, then set the final target.
        auto old = world.ReadTransform(camera);
        auto safe = *c;
        safe.target = {eye.x + f.x * 2, eye.y + f.y * 2, eye.z + f.z * 2};
        if (safe.target.x == old.position.x && safe.target.y == old.position.y &&
            safe.target.z == old.position.z)
            safe.target.y += 1;
        world.SetCamera(camera, safe);
        old.position = eye;
        world.SetTransform(camera, old);
        world.SetCamera(camera, *c);
    }
};
class FreeCameraController : public FPSCameraController
{
  public:
    float speed = 8;
    void Move(World &world, Entity camera, Vec3 local, double dt) const
    {
        if (!std::isfinite(dt) || dt < 0 || !std::isfinite(speed) || speed < 0 ||
            !std::isfinite(local.x) || !std::isfinite(local.y) || !std::isfinite(local.z))
            throw std::invalid_argument("invalid free camera movement");
        auto f = look.Forward();
        constexpr float r = 3.14159265358979323846f / 180;
        Vec3 right{-std::sin(look.yaw * r), 0, std::cos(look.yaw * r)};
        float length = std::sqrt(local.x * local.x + local.y * local.y + local.z * local.z);
        if (length > 1)
        {
            local.x /= length;
            local.y /= length;
            local.z /= length;
        }
        auto p = world.ReadTransform(camera).position;
        float step = float(dt) * speed;
        Update(world, camera,
               {p.x + (right.x * local.x + f.x * local.z) * step,
                p.y + (local.y + f.y * local.z) * step,
                p.z + (right.z * local.x + f.z * local.z) * step});
    }
};
} // namespace PlagC
