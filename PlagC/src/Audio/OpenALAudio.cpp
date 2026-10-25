#include "pgcpch.h"
#include <PlagC/Audio/AudioSystem.h>
#include <PlagC/Thread.h>
#include <limits>
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
namespace PlagC
{
namespace
{
std::atomic<std::uint64_t> nextSource{1};
void Check()
{
    if (auto error = alGetError(); error != AL_NO_ERROR)
        throw std::runtime_error("OpenAL error " + std::to_string(error));
}
bool Finite(Vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
void Validate(const AudioSourceDesc &d)
{
    if (!Finite(d.position) || !Finite(d.velocity) || !std::isfinite(d.gain) || d.gain < 0 ||
        !std::isfinite(d.pitch) || d.pitch <= 0 || !std::isfinite(d.referenceDistance) ||
        d.referenceDistance <= 0 || !std::isfinite(d.maxDistance) ||
        d.maxDistance < d.referenceDistance)
        throw std::invalid_argument("invalid audio source");
}
} // namespace
struct AudioSystem::Impl
{
    ALCdevice *device = nullptr;
    ALCcontext *context = nullptr;
    bool loopback = false;
    std::string error;
    struct Buffer
    {
        ALuint id = 0;
        AssetHandle<AudioClip> clip;
    };
    struct Source
    {
        ALuint id;
        const AudioClip *key;
    };
    std::unordered_map<const AudioClip *, Buffer> buffers;
    std::unordered_map<std::uint64_t, Source> sources;
    void Current() const
    {
        Thread::RequireMain();
        if (!context)
            throw std::runtime_error("audio unavailable: " + error);
        if (!alcMakeContextCurrent(context))
            throw std::runtime_error("OpenAL context activation failed");
    }
    ~Impl()
    {
        Thread::RequireMain();
        if (context)
        {
            alcMakeContextCurrent(context);
            for (auto &[id, s] : sources)
                alDeleteSources(1, &s.id);
            for (auto &[id, b] : buffers)
                alDeleteBuffers(1, &b.id);
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(context);
        }
        if (device)
            alcCloseDevice(device);
    }
};
AudioSystem::AudioSystem(const AudioConfig &config) : impl_(std::make_unique<Impl>())
{
    Thread::RequireMain();
    auto &p = *impl_;
    if (!config.enabled)
    {
        p.error = "audio disabled";
        return;
    }
    if (config.sampleRate < 8000 || config.sampleRate > 192000)
        throw std::invalid_argument("invalid audio sample rate");
    p.loopback = config.loopback;
    p.device = config.loopback ? alcLoopbackOpenDeviceSOFT(nullptr) : alcOpenDevice(nullptr);
    if (!p.device)
    {
        p.error = "no audio device";
        return;
    }
    const ALCint loopAttrs[] = {ALC_FORMAT_CHANNELS_SOFT,
                                ALC_STEREO_SOFT,
                                ALC_FORMAT_TYPE_SOFT,
                                ALC_SHORT_SOFT,
                                ALC_FREQUENCY,
                                ALCint(config.sampleRate),
                                0};
    const ALCint attrs[] = {ALC_FREQUENCY, ALCint(config.sampleRate), 0};
    p.context = alcCreateContext(p.device, config.loopback ? loopAttrs : attrs);
    if (!p.context)
    {
        p.error = "audio context creation failed";
        return;
    }
    p.Current();
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    Check();
}
AudioSystem::~AudioSystem() = default;
bool AudioSystem::Available() const
{
    Thread::RequireMain();
    return impl_->context != nullptr;
}
std::string AudioSystem::Error() const
{
    return impl_->error;
}
AudioSource AudioSystem::CreateSource(AssetHandle<AudioClip> clip, const AudioSourceDesc &desc)
{
    auto &p = *impl_;
    p.Current();
    Validate(*clip);
    Validate(desc);
    if (desc.spatial && clip->channels != 1)
        throw std::invalid_argument("positional audio requires mono PCM");
    auto key = clip.Shared().get();
    auto it = p.buffers.find(key);
    if (it == p.buffers.end())
    {
        ALuint buffer = 0;
        alGenBuffers(1, &buffer);
        Check();
        try
        {
            if (clip->samples.size() > std::size_t(std::numeric_limits<ALsizei>::max()) / 2)
                throw std::runtime_error("audio clip exceeds OpenAL size");
            alBufferData(buffer, clip->channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16,
                         clip->samples.data(), ALsizei(clip->samples.size() * 2),
                         ALsizei(clip->sampleRate));
            Check();
            it = p.buffers.emplace(key, Impl::Buffer{buffer, clip}).first;
        }
        catch (...)
        {
            alDeleteBuffers(1, &buffer);
            throw;
        }
    }
    ALuint source = 0;
    alGenSources(1, &source);
    Check();
    auto id = nextSource.fetch_add(1);
    try
    {
        alSourcei(source, AL_BUFFER, ALint(it->second.id));
        Check();
        p.sources.emplace(id, Impl::Source{source, key});
        UpdateSource({id}, desc);
    }
    catch (...)
    {
        p.sources.erase(id);
        alDeleteSources(1, &source);
        throw;
    }
    return {id};
}
void AudioSystem::DestroySource(AudioSource handle)
{
    auto &p = *impl_;
    p.Current();
    auto it = p.sources.find(handle.value);
    if (it == p.sources.end())
        throw std::invalid_argument("stale audio source");
    alDeleteSources(1, &it->second.id);
    Check();
    p.sources.erase(it);
}
void AudioSystem::UpdateSource(AudioSource handle, const AudioSourceDesc &d)
{
    auto &p = *impl_;
    p.Current();
    Validate(d);
    auto s = p.sources.at(handle.value);
    if (d.spatial && p.buffers.at(s.key).clip->channels != 1)
        throw std::invalid_argument("positional audio requires mono");
    alSource3f(s.id, AL_POSITION, d.position.x, d.position.y, d.position.z);
    alSource3f(s.id, AL_VELOCITY, d.velocity.x, d.velocity.y, d.velocity.z);
    alSourcef(s.id, AL_GAIN, d.gain);
    alSourcef(s.id, AL_PITCH, d.pitch);
    alSourcef(s.id, AL_REFERENCE_DISTANCE, d.referenceDistance);
    alSourcef(s.id, AL_MAX_DISTANCE, d.maxDistance);
    alSourcei(s.id, AL_LOOPING, d.loop);
    alSourcei(s.id, AL_SOURCE_RELATIVE, !d.spatial);
    alSourcef(s.id, AL_ROLLOFF_FACTOR, d.spatial ? 1.f : 0.f);
    Check();
}
void AudioSystem::Play(AudioSource h)
{
    impl_->Current();
    alSourcePlay(impl_->sources.at(h.value).id);
    Check();
}
void AudioSystem::Pause(AudioSource h)
{
    impl_->Current();
    alSourcePause(impl_->sources.at(h.value).id);
    Check();
}
void AudioSystem::Stop(AudioSource h)
{
    impl_->Current();
    alSourceStop(impl_->sources.at(h.value).id);
    Check();
}
AudioPlayback AudioSystem::State(AudioSource h) const
{
    impl_->Current();
    ALint value = 0;
    alGetSourcei(impl_->sources.at(h.value).id, AL_SOURCE_STATE, &value);
    Check();
    return value == AL_PLAYING  ? AudioPlayback::Playing
           : value == AL_PAUSED ? AudioPlayback::Paused
                                : AudioPlayback::Stopped;
}
void AudioSystem::SetListener(const AudioListener &l)
{
    impl_->Current();
    if (!Finite(l.position) || !Finite(l.velocity) || !Finite(l.forward) || !Finite(l.up) ||
        !std::isfinite(l.gain) || l.gain < 0)
        throw std::invalid_argument("invalid listener");
    auto cross = Vec3{l.forward.y * l.up.z - l.forward.z * l.up.y,
                      l.forward.z * l.up.x - l.forward.x * l.up.z,
                      l.forward.x * l.up.y - l.forward.y * l.up.x};
    if (cross.x * cross.x + cross.y * cross.y + cross.z * cross.z < 1e-8f)
        throw std::invalid_argument("degenerate listener orientation");
    float orientation[] = {l.forward.x, l.forward.y, l.forward.z, l.up.x, l.up.y, l.up.z};
    alListener3f(AL_POSITION, l.position.x, l.position.y, l.position.z);
    alListener3f(AL_VELOCITY, l.velocity.x, l.velocity.y, l.velocity.z);
    alListenerfv(AL_ORIENTATION, orientation);
    alListenerf(AL_GAIN, l.gain);
    Check();
}
void AudioSystem::CollectUnused()
{
    auto &p = *impl_;
    p.Current();
    std::erase_if(p.buffers, [&](auto &pair) {
        for (auto &[id, s] : p.sources)
            if (s.key == pair.first)
                return false;
        alDeleteBuffers(1, &pair.second.id);
        Check();
        return true;
    });
}
AudioStats AudioSystem::Statistics() const
{
    Thread::RequireMain();
    return {impl_->buffers.size(), impl_->sources.size(), Available()};
}
std::vector<std::int16_t> AudioSystem::RenderLoopback(unsigned frames)
{
    impl_->Current();
    if (!impl_->loopback || frames > 1920000)
        throw std::invalid_argument("invalid loopback render");
    std::vector<std::int16_t> samples(std::size_t(frames) * 2);
    alcRenderSamplesSOFT(impl_->device, samples.data(), ALCsizei(frames));
    if (alcGetError(impl_->device) != ALC_NO_ERROR)
        throw std::runtime_error("OpenAL loopback failed");
    return samples;
}
} // namespace PlagC
