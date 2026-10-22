#include "pgcpch.h"
#include "RenderDemo.h"
using namespace PlagC;
namespace
{
void Check(bool v, const char *text)
{
    if (!v)
        throw std::runtime_error(text);
    std::cout << "[PASS] " << text << '\n';
}
template <class F> bool Rejected(F fn)
{
    try
    {
        fn();
    }
    catch (const std::exception &)
    {
        return true;
    }
    return false;
}
void GeometryChecks(const MeshData &m)
{
    Check(!m.vertices.empty() && !m.indices.empty() && m.indices.size() % 3 == 0,
          "primitive has vertices and triangles");
    bool valid = true;
    for (std::size_t i = 0; i < m.indices.size(); i += 3)
    {
        const auto &a = m.vertices.at(m.indices[i]);
        const auto &b = m.vertices.at(m.indices[i + 1]);
        const auto &c = m.vertices.at(m.indices[i + 2]);
        Vec3 u{b.position.x - a.position.x, b.position.y - a.position.y,
               b.position.z - a.position.z},
            v{c.position.x - a.position.x, c.position.y - a.position.y,
              c.position.z - a.position.z};
        Vec3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
        valid = valid && (n.x * a.normal.x + n.y * a.normal.y + n.z * a.normal.z) > 0;
    }
    Check(valid, "primitive triangles have outward winding and nonzero area");
}
} // namespace
void RenderDemo::Start(Renderer &r, bool native)
{
    native_ = native;
    for (auto kind : {Primitive::Cube, Primitive::Plane, Primitive::Sphere, Primitive::Cylinder})
    {
        auto data = MakePrimitive(kind);
        GeometryChecks(data);
        meshes_.push_back(r.CreateMesh(data));
    }
    MaterialDesc red;
    red.colour = {.8f, .15f, .08f};
    materials_.push_back(r.CreateMaterial(red));
    MaterialDesc tiled;
    tiled.colour = {1, 1, 1};
    tiled.colourTexture = "checker.ppm";
    materials_.push_back(r.CreateMaterial(tiled));
    MaterialDesc metal;
    metal.colour = {.75f, .8f, .9f};
    metal.metallic = .8f;
    metal.roughness = .22f;
    materials_.push_back(r.CreateMaterial(metal));
    MaterialDesc unlit;
    unlit.model = MaterialModel::Unlit;
    unlit.colour = {.1f, .65f, 1};
    materials_.push_back(r.CreateMaterial(unlit));
    r.UpdateMaterial(materials_[0], red);
    objects_.push_back(
        r.CreateObject(meshes_[0], materials_[0], {{-2, 1, 0}, {0, 25, 0}, {2, 2, 2}}));
    objects_.push_back(r.CreateObject(meshes_[1], materials_[1], {{0, -.02f, 0}, {}, {16, 1, 16}}));
    objects_.push_back(r.CreateObject(meshes_[2], materials_[2], {{1, 1, 0}, {}, {2, 2, 2}}));
    objects_.push_back(r.CreateObject(meshes_[3], materials_[3], {{4, 1, 0}, {}, {1.5f, 2, 1.5f}}));
    r.SetVisible(objects_[0], false);
    r.SetVisible(objects_[0], true);
    r.SetCastShadows(objects_[0], false);
    r.SetCastShadows(objects_[0], true);
    r.SetRenderQueue(objects_[0], 10);
    Check(Rejected([&] { r.DestroyMesh(meshes_[0]); }) &&
              Rejected([&] { r.DestroyMaterial(materials_[0]); }),
          "in-use renderer resources cannot be destroyed");
    auto disposable = r.CreateObject(meshes_[0], materials_[0]);
    r.DestroyObject(disposable);
    Check(Rejected([&] { r.SetVisible(disposable, true); }), "stale renderer handle is rejected");
    MeshData bad = MakePrimitive(Primitive::Cube);
    bad.indices[0] = 99999;
    Check(Rejected([&] { r.CreateMesh(bad); }), "invalid mesh index rejected before GPU upload");
    std::atomic<bool> threadRejected{false};
    std::thread wrong(
        [&] { threadRejected = Rejected([&] { r.SetVisible(objects_[0], false); }); });
    wrong.join();
    Check(threadRejected, "renderer enforces main-thread ownership");
    LightDesc sun;
    sun.direction = {.6f, -1, .6f};
    lights_.push_back(r.CreateLight(sun));
    LightDesc point;
    point.type = LightType::Point;
    point.position = {0, 4, 3};
    point.colour = {1, .6f, .3f};
    point.power = 15;
    lights_.push_back(r.CreateLight(point));
    LightDesc spot;
    spot.type = LightType::Spot;
    spot.position = {4, 6, 4};
    spot.direction = {-.3f, -1, -1};
    spot.colour = {.3f, .5f, 1};
    spot.power = 20;
    lights_.push_back(r.CreateLight(spot));
    r.UpdateLight(lights_.back(), spot);
    r.SetCamera({{10, 7, 12}, {0, 1, 0}, 55, .1f, 100});
    meshes_.push_back(r.LoadModel("robot.mesh"));
    objects_.push_back(
        r.CreateObject(meshes_.back(), materials_[2], {{-4, 0, 2}, {}, {.03f, .03f, .03f}}));
    auto animations = r.Animations(objects_.back());
    Check(!animations.empty(), "legacy model imports skeletal animation");
    const std::string walk =
        std::find(animations.begin(), animations.end(), "Walk") != animations.end()
            ? "Walk"
            : animations.front();
    r.PlayAnimation(objects_.back(), walk);
    r.StopAnimation(objects_.back());
    r.PlayAnimation(objects_.back(), walk, true, 1);
    Check(Rejected([&] { r.PlayAnimation(objects_[0], "missing"); }),
          "animation request on static mesh is rejected");
    target_ = r.CreateTarget(320, 180);
    r.ResizeTarget(target_, 640, 360);
    Check(Rejected([&] { r.ResizeTarget(target_, 0, 1); }),
          "invalid target resize preserves live target");
    r.SetSettings(settings_);
    r.Resize(0, 0);
    auto before = r.Statistics().frames;
    r.Render(.016);
    Check(r.Statistics().frames == before, "zero-size framebuffer pauses rendering");
    r.Resize(960, 540);
    auto s = r.Statistics();
    Check(s.objects == 5 && s.meshes == 5 && s.materials == 4 && s.lights == 3,
          "scene resource counts match client ownership");
    std::cout << "[INFO] Scene ready; W wireframe, S shadows, Escape closes\n";
}
void RenderDemo::Frame(Renderer &r, double dt)
{
    time_ += dt;
    ++frame_;
    // Keep geometry stationary while collecting comparison images.
    if (frame_ > 5)
        r.SetTransform(objects_[0], {{-2, 1, 0}, {0, float(time_ * 30), 0}, {2, 2, 2}});
    if (frame_ == 2)
    {
        if (native_)
            r.CaptureTarget(target_, "Logs/Phase2-shadows.ppm");
        settings_.shadows = false;
        settings_.vsync = false;
        r.SetSettings(settings_);
    }
    if (frame_ == 3)
    {
        if (native_)
            r.CaptureTarget(target_, "Logs/Phase2-no-shadows.ppm");
        settings_.wireframe = true;
        r.SetSettings(settings_);
    }
    if (frame_ == 4)
    {
        Check(r.Statistics().frames >= 3, "Ogre advances frames through Sandbox");
        if (native_)
            r.CaptureTarget(target_, "Logs/Phase2-wireframe.ppm");
        settings_ = {};
        r.SetSettings(settings_);
    }
    if (frame_ == 5 && native_)
    {
        r.CaptureTarget(target_, "Logs/Phase2.ppm");
        Check(std::filesystem::file_size("Logs/Phase2.ppm") > 640 * 360 * 3,
              "offscreen target produces a capture");
    }
}
void RenderDemo::ToggleWireframe(Renderer &r)
{
    settings_.wireframe = !settings_.wireframe;
    r.SetSettings(settings_);
}
void RenderDemo::ToggleShadows(Renderer &r)
{
    settings_.shadows = !settings_.shadows;
    r.SetSettings(settings_);
}
void RenderDemo::Stop(Renderer &r)
{
    for (auto h : objects_)
    {
        r.DestroyObject(h);
    }
    objects_.clear();
    for (auto h : lights_)
    {
        r.DestroyLight(h);
    }
    lights_.clear();
    for (auto h : materials_)
    {
        r.DestroyMaterial(h);
    }
    materials_.clear();
    for (auto h : meshes_)
    {
        r.DestroyMesh(h);
    }
    meshes_.clear();
    r.DestroyTarget(target_);
    target_ = {};
    auto s = r.Statistics();
    Check(!s.objects && !s.materials && !s.meshes && !s.lights,
          "client releases every scene resource before renderer shutdown");
}
