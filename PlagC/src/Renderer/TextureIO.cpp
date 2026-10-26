#include "pgcpch.h"
#include <PlagC/Renderer/Renderer.h>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#endif

namespace PlagC
{
TextureHandle Renderer::LoadTexture(const std::string &resource, bool srgb, bool mipmaps)
{
#ifdef _WIN32
    // Runtime assets are staged beside the executable, so authored textures resolve from the
    // engine-owned Assets/Textures directory without exposing Ogre types to the game layer.
    std::filesystem::path path = std::filesystem::path("Assets") / "Textures" / resource;
    if (!std::filesystem::is_regular_file(path))
        throw std::runtime_error("texture not found: " + path.string());

    HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(init);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE)
        throw std::runtime_error("failed to initialize COM for texture decode");

    IWICImagingFactory *factory = nullptr;
    IWICBitmapDecoder *decoder = nullptr;
    IWICBitmapFrameDecode *frame = nullptr;
    IWICFormatConverter *converter = nullptr;
    auto release = [&]() {
        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (factory) factory->Release();
        if (uninitialize) CoUninitialize();
    };

    try
    {
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
        if (FAILED(hr)) throw std::runtime_error("failed to create WIC factory");
        auto wide = path.wstring();
        hr = factory->CreateDecoderFromFilename(wide.c_str(), nullptr, GENERIC_READ,
                                                WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) throw std::runtime_error("failed to decode texture: " + path.string());
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) throw std::runtime_error("texture has no image frame: " + path.string());
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) throw std::runtime_error("failed to create texture converter");
        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                   nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) throw std::runtime_error("failed to convert texture to RGBA8");
        UINT width = 0, height = 0;
        converter->GetSize(&width, &height);
        if (!width || !height || width > 8192 || height > 8192)
            throw std::runtime_error("unsupported texture dimensions: " + path.string());
        std::vector<std::uint8_t> rgba(std::size_t(width) * height * 4);
        hr = converter->CopyPixels(nullptr, width * 4, UINT(rgba.size()), rgba.data());
        if (FAILED(hr)) throw std::runtime_error("failed to read texture pixels");
        release();
        return CreateTexture(width, height, rgba, srgb, mipmaps);
    }
    catch (...)
    {
        release();
        throw;
    }
#else
    (void)resource;
    (void)srgb;
    (void)mipmaps;
    throw std::runtime_error("authored texture loading is not implemented on this platform");
#endif
}
} // namespace PlagC
