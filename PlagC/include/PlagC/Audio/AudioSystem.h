#pragma once
#include <PlagC/Assets/Asset.h>
namespace PlagC
{
struct AudioSource
{
    std::uint64_t value = 0;
    explicit operator bool() const
    {
        return value != 0;
    }
};
struct AudioSourceDesc
{
    Vec3 position{}, velocity{};
    float gain = 1, pitch = 1, referenceDistance = 1, maxDistance = 100;
    bool loop = false, spatial = true;
};
struct AudioListener
{
    Vec3 position{}, velocity{}, forward{0, 0, -1}, up{0, 1, 0};
    float gain = 1;
};
enum class AudioPlayback
{
    Stopped,
    Playing,
    Paused
};
struct AudioStats
{
    std::size_t buffers = 0, sources = 0;
    bool available = false;
};
struct AudioConfig
{
    bool enabled = true, loopback = false;
    unsigned sampleRate = 48000;
};
// Owner-thread OpenAL context. Sources retain immutable clip handles until destroyed.
class AudioSystem
{
  public:
    explicit AudioSystem(const AudioConfig & = {});
    ~AudioSystem();
    AudioSystem(const AudioSystem &) = delete;
    AudioSystem &operator=(const AudioSystem &) = delete;
    bool Available() const;
    std::string Error() const;
    AudioSource CreateSource(AssetHandle<AudioClip>, const AudioSourceDesc & = {});
    void DestroySource(AudioSource);
    void UpdateSource(AudioSource, const AudioSourceDesc &);
    void Play(AudioSource);
    void Pause(AudioSource);
    void Stop(AudioSource);
    AudioPlayback State(AudioSource) const;
    void SetListener(const AudioListener &);
    AudioStats Statistics() const;
    void CollectUnused();
    std::vector<std::int16_t> RenderLoopback(
        unsigned frames); // stereo PCM; deterministic headless validation
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace PlagC
