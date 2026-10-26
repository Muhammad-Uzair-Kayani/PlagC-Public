// Real Ogre/OpenGL vertical slice: cooked external model + authored texture + PBR/IBL/HDR.
#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
#include <PlagC/Assets/AssetScene.h>
#include <dlfcn.h>
using namespace PlagC;
namespace
{
void ValidateCurrentProgram()
{
    auto getError = reinterpret_cast<unsigned (*)()>(dlsym(RTLD_DEFAULT, "glGetError"));
    auto getInteger =
        reinterpret_cast<void (*)(unsigned, int *)>(dlsym(RTLD_DEFAULT, "glGetIntegerv"));
    auto validate = reinterpret_cast<void (*)(unsigned)>(dlsym(RTLD_DEFAULT, "glValidateProgram"));
    auto getProgram = reinterpret_cast<void (*)(unsigned, unsigned, int *)>(
        dlsym(RTLD_DEFAULT, "glGetProgramiv"));
    if (!getError || !getInteger || !validate || !getProgram)
        throw std::runtime_error("GL validation entry points unavailable");
    if (auto error = getError())
        throw std::runtime_error("OpenGL draw error: " + std::to_string(error));
    int program = 0;
    getInteger(0x8B8D, &program); // GL_CURRENT_PROGRAM
    if (!program)
        throw std::runtime_error("no linked draw program");
    validate(unsigned(program));
    int valid = 0;
    getProgram(unsigned(program), 0x8B83, &valid);
    if (!valid || getError())
        throw std::runtime_error("draw-state GLSL validation failed");
}
void VerifyConflictingBindingsAreDetected()
{
    auto integer =
        reinterpret_cast<void (*)(unsigned, int *)>(dlsym(RTLD_DEFAULT, "glGetIntegerv"));
    auto active =
        reinterpret_cast<void (*)(unsigned, unsigned, int, int *, int *, unsigned *, char *)>(
            dlsym(RTLD_DEFAULT, "glGetActiveUniform"));
    auto location = reinterpret_cast<int (*)(unsigned, const char *)>(
        dlsym(RTLD_DEFAULT, "glGetUniformLocation"));
    auto getUniform =
        reinterpret_cast<void (*)(unsigned, int, int *)>(dlsym(RTLD_DEFAULT, "glGetUniformiv"));
    auto setUniform = reinterpret_cast<void (*)(int, int)>(dlsym(RTLD_DEFAULT, "glUniform1i"));
    auto validate = reinterpret_cast<void (*)(unsigned)>(dlsym(RTLD_DEFAULT, "glValidateProgram"));
    auto programInfo = reinterpret_cast<void (*)(unsigned, unsigned, int *)>(
        dlsym(RTLD_DEFAULT, "glGetProgramiv"));
    if (!integer || !active || !location || !getUniform || !setUniform || !validate || !programInfo)
        throw std::runtime_error("missing GL negative-control entry points");
    int program = 0, count = 0;
    integer(0x8B8D, &program);
    programInfo(unsigned(program), 0x8B86, &count);
    int firstLocation = -1, secondLocation = -1;
    unsigned firstType = 0;
    for (int i = 0; i < count; ++i)
    {
        char name[256];
        int length = 0, size = 0;
        unsigned type = 0;
        active(unsigned(program), unsigned(i), 256, &length, &size, &type, name);
        if (type != 0x8B60 && type != 0x8DC1 && type != 0x8DC2 && type != 0x8B62 && type != 0x8B5E)
            continue;
        auto loc = location(unsigned(program), name);
        if (loc < 0)
            continue;
        if (firstLocation < 0)
        {
            firstLocation = loc;
            firstType = type;
        }
        else if (type != firstType)
        {
            secondLocation = loc;
            break;
        }
    }
    if (secondLocation < 0)
        throw std::runtime_error("inspection did not exercise mixed sampler types");
    int firstUnit = 0, original = 0;
    getUniform(unsigned(program), firstLocation, &firstUnit);
    getUniform(unsigned(program), secondLocation, &original);
    setUniform(secondLocation, firstUnit);
    validate(unsigned(program));
    int valid = 1;
    programInfo(unsigned(program), 0x8B83, &valid);
    setUniform(secondLocation, original);
    ValidateCurrentProgram();
    if (valid)
        throw std::runtime_error("negative control did not reject a real sampler collision");
    std::cout << "PASS: negative control detects conflicting draw bindings\n";
}

} // namespace
int main(int argc, char **argv)
{
    Thread::InitializeMain();
    try
    {
        EngineConfig cfg;
        cfg.offscreen = true;
        cfg.workers = 2;
        cfg.audioEnabled = false;
        Engine engine(cfg);
        auto &r = engine.Graphics();
        auto asset =
            engine.Assets().Load<ModelAsset>(argc > 1 ? argv[1] : "TPS/AAA/SecurityCamera.plasset");
        Validate(*asset);
        if (asset->textures.empty())
            throw std::runtime_error("external model lost its textures");
        Animator rest(asset);
        Vec3 lo{1e20f, 1e20f, 1e20f}, hi{-1e20f, -1e20f, -1e20f};
        for (unsigned i = 0; i < asset->meshes.size(); ++i)
            for (auto v : rest.Deform(i).vertices)
            {
                lo = {std::min(lo.x, v.position.x), std::min(lo.y, v.position.y),
                      std::min(lo.z, v.position.z)};
                hi = {std::max(hi.x, v.position.x), std::max(hi.y, v.position.y),
                      std::max(hi.z, v.position.z)};
            }
        float scale = 2 / std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z});
        World world;
        auto entity = world.Create("Inspection prop");
        world.SetTransform(
            entity, {{-(hi.x + lo.x) * .5f * scale, -lo.y * scale, -(hi.z + lo.z) * .5f * scale},
                     {},
                     {scale, scale, scale}});
        world.SetModel(entity, {asset});
        AssetScene bridge(r);
        bridge.Update(world, 0);
        auto texture = r.LoadTexture("SliceDiffuse.jpg", true, true);
        MaterialDesc material;
        material.baseColourTexture = texture;
        material.roughness = .6f;
        auto floorMaterial = r.CreateMaterial(material);
        auto plane = r.CreateMesh(MakePrimitive(Primitive::Plane));
        auto floor = r.CreateObject(plane, floorMaterial, {{0, -.02f, 0}, {}, {6, 1, 6}});
        LightDesc desc;
        desc.direction = {-.5f, -1, -.3f};
        auto light = r.CreateLight(desc);
        r.SetCamera({{3, 2.4f, 4}, {0, .7f, 0}, 48, .05f, 100});
        auto target = r.CreateTarget(960, 640);
        // First exercise the PBR program itself, not just the final tone-map program.
        r.SetEnvironment({"PlagCEnvironment.dds", false, true});
        for (int i = 0; i < 4; ++i)
        {
            r.Render(1. / 60);
            ValidateCurrentProgram();
        }
        VerifyConflictingBindingsAreDetected();
        r.CaptureTarget(target, "Logs/AssetSlice-PBR.ppm");
        auto settings = r.Settings();
        settings.hdr = true;
        r.SetSettings(settings);
        r.SetEnvironment({"PlagCEnvironment.dds", true, true});
        for (int i = 0; i < 4; ++i)
        {
            r.Render(1. / 60);
            ValidateCurrentProgram();
        }
        r.CaptureTarget(target, "Logs/AssetSlice-HDR.ppm");
        r.ResizeTarget(target, 640, 480);
        r.Render(1. / 60);
        ValidateCurrentProgram();
        r.DestroyTarget(target);
        bridge.Clear();
        r.DestroyLight(light);
        r.DestroyObject(floor);
        r.DestroyMesh(plane);
        r.DestroyMaterial(floorMaterial);
        r.DestroyTexture(texture);
        auto stats = r.Statistics();
        if (stats.objects || stats.meshes || stats.materials || stats.textures || stats.lights)
            throw std::runtime_error("inspection resource leak");
        std::cout << "PASS: external cooked model, textures, PBR/IBL, HDR/sky, resize, real GL "
                     "validation and resource cleanup\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
