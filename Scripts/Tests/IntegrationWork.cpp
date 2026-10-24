#include "pgcpch.h"
#include "IntegrationWork.h"
#include <type_traits>
template<class T> unsigned Animate(PlagC::JobSystem& jobs, std::span<T> updates,
                            float dt, float time, unsigned iterations) {
    if (updates.empty()) return 0;
    struct Chunk {
        T* data;
        std::size_t begin, end;
        float dt, time;
        unsigned iterations;
    };
    const auto count = std::min<std::size_t>({updates.size(), std::size_t(jobs.WorkerCount()) * 4, 256});
    std::vector<Chunk> chunks(count);
    PlagC::JobGroup group;
    struct Drain { PlagC::JobSystem& jobs; PlagC::JobGroup& group; ~Drain() { jobs.Wait(group); } } drain{jobs, group};
    for (std::size_t i = 0; i < count; ++i) {
        chunks[i] = {updates.data(), updates.size() * i / count, updates.size() * (i + 1) / count, dt, time, iterations};
        for (;;) {
            const auto result = jobs.TrySubmit(group, [](void* p) {
                const auto& c = *static_cast<Chunk*>(p);
                for (auto j = c.begin; j < c.end; ++j) {
                    auto& t = [&]() -> PlagC::Transform& {
                        if constexpr (std::is_same_v<T,PlagC::Transform>) return c.data[j];
                        else return c.data[j].transform;
                    }();
                    float wave = c.time + float(j) * .07f;
                    // Optional extra useful-output math for CPU load exploration.
                    // Every iteration feeds the result, so optimizers cannot discard it.
                    for (unsigned k = 0; k < c.iterations; ++k) wave = std::sin(wave + .01f * float(k));
                    t.position.y = 1.2f + .5f * wave;
                    t.rotationDegrees.y = std::fmod(t.rotationDegrees.y + 45 * c.dt, 360.f);
                }
            }, &chunks[i]);
            if (result == PlagC::SubmitResult::Accepted) break;
            if (result == PlagC::SubmitResult::Stopped) throw std::runtime_error("integration jobs stopped");
            jobs.Wait(group);
        }
    }
    jobs.Wait(group);
    if (group.failed.load()) throw std::runtime_error("integration animation job failed");
    return static_cast<unsigned>(count);
}

unsigned AnimateIntegration(PlagC::JobSystem& jobs,std::vector<PlagC::TransformUpdate>& values,float dt,float time,unsigned iterations) {
    return Animate(jobs,std::span<PlagC::TransformUpdate>(values),dt,time,iterations);
}
unsigned AnimateTransforms(PlagC::JobSystem& jobs,std::span<PlagC::Transform> values,float dt,float time,unsigned iterations) {
    return Animate(jobs,values,dt,time,iterations);
}
