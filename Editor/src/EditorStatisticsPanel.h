#pragma once

#include <PlagC/Jobs.h>
#include <PlagC/Profiler.h>
#include <PlagC/Renderer/Backend.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace PlagC {
class Engine;
}

namespace PlagC::Editor {
class EditorSession;

class EditorStatisticsPanel {
public:
    void Draw(Engine&, EditorSession&);
    void Reset() noexcept;

private:
    static constexpr std::size_t HistoryCapacity = 240;

    struct Series {
        std::array<float, HistoryCapacity> values{};
        std::size_t cursor = 0;
        std::size_t count = 0;

        void Push(float value) noexcept;
        void Clear() noexcept;
        int PlotOffset() const noexcept;
    };

    void Capture(Engine&, EditorSession&);
    void ClearHistory() noexcept;

    bool frozen_ = false;
    bool haveSnapshot_ = false;
    FrameProfile frame_{};
    JobStats jobs_{};
    RenderStats render_{};
    RendererBackend backend_ = RendererBackend::Auto;
    std::size_t entities_ = 0;
    std::size_t assets_ = 0;
    unsigned workers_ = 0;
    double jobUtilization_ = 0.0;
    std::uint64_t previousBusyNanoseconds_ = 0;
    std::chrono::steady_clock::time_point previousJobSample_{};

    Series frameMsHistory_{};
    Series fpsHistory_{};
    Series gpuMsHistory_{};
    Series jobUtilizationHistory_{};
};

} // namespace PlagC::Editor
