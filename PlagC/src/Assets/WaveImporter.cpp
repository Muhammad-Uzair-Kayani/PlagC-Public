#include "pgcpch.h"
#include <PlagC/Assets/Asset.h>
#include <bit>
#include <cstring>
namespace PlagC
{
AudioClip ImportWave(const std::filesystem::path &path)
{
    auto size = std::filesystem::file_size(path);
    if (size < 12 || size > 128 * 1024 * 1024)
        throw std::runtime_error("invalid WAV size");
    std::vector<unsigned char> data(size);
    std::ifstream f(path, std::ios::binary);
    f.read(reinterpret_cast<char *>(data.data()), std::streamsize(size));
    if (!f)
        throw std::runtime_error("WAV read failed");
    auto u16 = [&](std::size_t p) {
        if (p + 2 > size)
            throw std::runtime_error("truncated WAV");
        return std::uint16_t(data[p] | std::uint16_t(data[p + 1]) << 8);
    };
    auto u32 = [&](std::size_t p) {
        return std::uint32_t(u16(p)) | std::uint32_t(u16(p + 2)) << 16;
    };
    if (std::memcmp(data.data(), "RIFF", 4) || std::memcmp(data.data() + 8, "WAVE", 4) ||
        std::uint64_t(u32(4)) + 8 != size)
        throw std::runtime_error("invalid RIFF/WAVE");
    AudioClip clip;
    bool format = false, pcm = false;
    for (std::size_t p = 12; p + 8 <= size;)
    {
        auto count = u32(p + 4);
        std::size_t start = p + 8;
        if (count > size - start)
            throw std::runtime_error("invalid WAV chunk");
        if (!std::memcmp(data.data() + p, "fmt ", 4))
        {
            if (count < 16 || u16(start) != 1 || u16(start + 14) != 16)
                throw std::runtime_error("WAV must be PCM16 mono/stereo");
            clip.channels = u16(start + 2);
            clip.sampleRate = u32(start + 4);
            if (u16(start + 12) != clip.channels * 2 ||
                u32(start + 8) != clip.sampleRate * clip.channels * 2)
                throw std::runtime_error("invalid WAV rate/alignment");
            format = true;
        }
        else if (!std::memcmp(data.data() + p, "data", 4))
        {
            if (pcm || count % 2)
                throw std::runtime_error("invalid duplicate/odd WAV data");
            pcm = true;
            clip.samples.reserve(count / 2);
            for (std::size_t i = 0; i < count; i += 2)
                clip.samples.push_back(std::bit_cast<std::int16_t>(u16(start + i)));
        }
        p = start + count + (count % 2);
    }
    if (!format || !pcm)
        throw std::runtime_error("missing WAV chunks");
    clip.metadata.type = AssetType::Audio;
    Validate(clip);
    return clip;
}
} // namespace PlagC
