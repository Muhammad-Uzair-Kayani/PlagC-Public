#include <PlagC/Renderer/Backend.h>
#include <cassert>
#include <iostream>
using namespace PlagC;
template <class F> void Reject(F fn)
{
    bool rejected = false;
    try
    {
        fn();
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    assert(rejected);
}
int main()
{
    const RendererBackendCapabilities native{true, false, true}, nullOnly{false, false, true};
    assert(ResolveRendererBackend(RendererBackend::Auto, native) == RendererBackend::OpenGL);
    assert(ResolveRendererBackend(RendererBackend::OpenGL, native) == RendererBackend::OpenGL);
    assert(ResolveRendererBackend(RendererBackend::Null, native) == RendererBackend::Null);
    assert(ResolveRendererBackend(RendererBackend::Auto, nullOnly, true) == RendererBackend::Null);
    assert(!native.Supports(RendererBackend::Vulkan));
    Reject([&] { ResolveRendererBackend(RendererBackend::Vulkan, native); });
    Reject([&] { ResolveRendererBackend(RendererBackend::Auto, nullOnly); });
    Reject([&] { ResolveRendererBackend(RendererBackend::OpenGL, native, true); });
    Reject([&] { ResolveRendererBackend(RendererBackend::Null, native, false, true); });
    Reject([&] { ResolveRendererBackend(RendererBackend(999), native); });
    assert(ParseRendererBackend("vulkan") == RendererBackend::Vulkan);
    assert(ParseRendererBackend("opengl") == RendererBackend::OpenGL);
    assert(ParseRendererBackend("auto") == RendererBackend::Auto);
    assert(ParseRendererBackend("null") == RendererBackend::Null);
    Reject([] { ParseRendererBackend("invalid"); });
    assert(ResolveRendererBackend(RendererBackend::Vulkan, {true, true, true}, false, true) == RendererBackend::Vulkan);
    Reject([] { ResolveRendererBackend(RendererBackend::Vulkan, {true, true, true}, true); });
    // Capability policy is independent of hardware availability. Auto prefers Vulkan,
    // but retains OpenGL as the fallback for builds that do not include Vulkan.
    assert(ResolveRendererBackend(RendererBackend::Auto, {false, true, true}) ==
           RendererBackend::Vulkan);
    assert(ResolveRendererBackend(RendererBackend::Auto, {true, true, true}) ==
           RendererBackend::Vulkan);
    assert(ResolveRendererBackend(RendererBackend::Auto, {true, false, true}) ==
           RendererBackend::OpenGL);
    std::cout << "Renderer backend policy checks passed\n";
}
