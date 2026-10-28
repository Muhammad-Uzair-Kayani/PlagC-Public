#include <PlagC/Application.h>
#include <PlagC/Thread.h>
#include <filesystem>
#include <iostream>
#include <vector>
int main(int argc, char** argv)
{
    PlagC::Thread::InitializeMain();
    try
    {
        PlagC::EngineConfig config;
        config.rendererBackend = PlagC::ParseRendererBackend(argc > 1 ? argv[1] : "vulkan");
        config.offscreen = argc < 3 || std::string(argv[2]) != "window";
        config.audioEnabled = false;
        config.workers = 2;
        PlagC::Engine engine(config);
        auto& r = engine.Graphics();
        std::filesystem::create_directories("Logs");
        const std::string prefix = "Logs/Backend-" + std::string(PlagC::BackendName(r.Backend()));
        auto settings = r.Settings();
        settings.hdr = true;
        settings.bloom = true;
        settings.fogDensity = .0018f;
        r.SetSettings(settings);
        r.SetEnvironment({"PlagCEnvironment.dds"});
        auto data = PlagC::MakePrimitive(PlagC::Primitive::Cube);
        auto mesh = r.CreateMesh(data, true);
        const std::vector<std::uint8_t> pixels = {220,60,30,255, 30,150,220,255, 30,150,220,255, 220,60,30,255};
        auto texture = r.CreateTexture(2, 2, pixels, true, true);
        PlagC::MaterialDesc material;
        material.baseColourTexture = texture;
        material.alphaCutoff = .5f; // Cutout shadows + reflection exercise the caster descriptor layout.
        material.emissive = {1.5f, .1f, .02f};
        auto mat = r.CreateMaterial(material);
        auto object = r.CreateObject(mesh, mat);
        auto light = r.CreateLight({});
        auto target = r.CreateTarget(640, 360);
        r.SetCamera({{3,2,4},{0,0,0}});
        for (int i=0; i<10; ++i) r.Render(1./60);
        r.CaptureTarget(target, prefix + "-hdr.ppm");
        for (auto& vertex : data.vertices) vertex.position.y *= 1.2f;
        r.UpdateMesh(mesh, data.vertices);
        settings.bloom = false;
        r.SetSettings(settings);
        r.ResizeTarget(target, 320, 180);
        r.Resize(800, 450);
        for (int i=0; i<3; ++i) r.Render(1./60);
        r.CaptureTarget(target, prefix + "-resize.ppm");
        settings.hdr = false;
        r.SetSettings(settings);
        for (int i=0; i<3; ++i) r.Render(1./60);
        r.CaptureTarget(target, prefix + "-ldr.ppm");
        r.DestroyTarget(target);
        r.DestroyObject(object);
        r.DestroyMesh(mesh);
        r.DestroyMaterial(mat);
        r.DestroyTexture(texture);
        r.DestroyLight(light);
        std::cout << "Backend render/resize/cleanup passed: " << PlagC::BackendName(r.Backend()) << '\n';
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
