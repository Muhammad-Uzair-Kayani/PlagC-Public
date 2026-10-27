#include "pgcpch.h"
#include "CombatEffects.h"
using namespace PlagC;
namespace Game
{
CombatEffects::CombatEffects(Renderer &r) : renderer_(r)
{
    mesh_ = r.CreateMesh(MakePrimitive(Primitive::Sphere));
    MaterialDesc d;
    d.colour = {.9f, .5f, .12f};
    d.emissive = {12, 4, .4f};
    d.roughness = .5f;
    sparks_ = r.CreateMaterial(d);
    d.colour = {.32f, .27f, .19f};
    d.emissive = {};
    d.roughness = 1;
    dust_ = r.CreateMaterial(d);
    LightDesc light;
    light.type = LightType::Point;
    light.colour = {1, .58f, .2f};
    light.radius = 5;
    light.power = 0;
    muzzle_ = r.CreateLight(light);
    for (auto &p : particles_)
    {
        p.object = r.CreateObject(mesh_, dust_);
        r.SetVisible(p.object, false);
        r.SetCastShadows(p.object, false);
    }
}
CombatEffects::~CombatEffects()
{
    for (auto &p : particles_)
        if (p.object)
            renderer_.DestroyObject(p.object);
    renderer_.DestroyLight(muzzle_);
    renderer_.DestroyMesh(mesh_);
    renderer_.DestroyMaterial(sparks_);
    renderer_.DestroyMaterial(dust_);
}
void CombatEffects::Muzzle(Vec3 position)
{
    muzzleTime_ = .045f;
    LightDesc light;
    light.type = LightType::Point;
    light.colour = {1, .58f, .2f};
    light.radius = 5;
    light.power = 9;
    light.position = position;
    renderer_.UpdateLight(muzzle_, light);
}
void CombatEffects::Impact(Vec3 position, Vec3 normal, bool metal)
{
    for (unsigned i = 0; i < (metal ? 9u : 6u); ++i)
    {
        auto &p = particles_[next_++ % particles_.size()];
        // Material changes are structural: reuse fixed pool slots by replacing only the object.
        renderer_.DestroyObject(p.object);
        p.object = renderer_.CreateObject(mesh_, metal ? sparks_ : dust_);
        renderer_.SetCastShadows(p.object, false);
        float a = float(i) * 2.399963f;
        p.position = {position.x + normal.x * .04f, position.y + normal.y * .04f,
                      position.z + normal.z * .04f};
        float speed = metal ? 3.5f : 1.4f;
        p.velocity = {normal.x * speed + std::cos(a) * speed * .65f,
                      normal.y * speed + .7f + float(i % 3) * .4f,
                      normal.z * speed + std::sin(a) * speed * .65f};
        p.life = p.initial = metal ? .18f + float(i % 4) * .06f : .4f + float(i % 3) * .12f;
        p.size = metal ? .018f : .035f;
        p.spark = metal;
        renderer_.SetTransform(p.object, {p.position, {}, {p.size, p.size, p.size}});
    }
}
void CombatEffects::Update(double dt)
{
    float t = std::min(float(dt), .1f);
    if (muzzleTime_ > 0)
    {
        muzzleTime_ = std::max(0.f, muzzleTime_ - t);
        if (muzzleTime_ == 0)
        {
            LightDesc d;
            d.type = LightType::Point;
            d.power = 0;
            d.radius = 5;
            renderer_.UpdateLight(muzzle_, d);
        }
    }
    for (auto &p : particles_)
        if (p.life > 0)
        {
            p.life = std::max(0.f, p.life - t);
            p.velocity.y -= (p.spark ? 7.f : 1.5f) * t;
            p.position.x += p.velocity.x * t;
            p.position.y += p.velocity.y * t;
            p.position.z += p.velocity.z * t;
            float size = p.size * (p.spark ? .5f + .5f * p.life / p.initial
                                           : 1 + 2 * (1 - p.life / p.initial));
            renderer_.SetTransform(p.object, {p.position, {}, {size, size, size}});
            renderer_.SetVisible(p.object, p.life > 0);
        }
}
} // namespace Game
