#include <PlagC/PerformanceCapture.h>
#include <cassert>
#include <sstream>
int main()
{
    using namespace PlagC;
    const auto path = std::filesystem::temp_directory_path() / "plagc-performance-test.csv";
    RenderStats stats;
    stats.drawCalls = 42;
    PerformanceCapture capture(path.string(), 2, .1);
    capture.Startup(10, 20, 1, 2, "native", true);
    auto frame = [&](double ms, bool minimized = false) {
        return capture.Frame(ms, 4, stats, 1, 2, 1, false, false, 1280, 720, minimized);
    };
    assert(!frame(900)); // warmup spikes must not enter percentiles or elapsed time.
    assert(!frame(900));
    assert(!frame(10));
    assert(!frame(20));
    assert(!frame(30));
    assert(frame(40, true)); // minimized frames count toward duration, excluded from summary.
    capture.Finish();
    capture.Finish();
    std::ifstream summary(path.string() + ".summary.txt");
    std::stringstream text;
    text << summary.rdbuf();
    assert(text.str().find("valid_frames=3") != std::string::npos);
    assert(text.str().find("frame_p50_ms=20") != std::string::npos);
    assert(text.str().find("frame_p99_ms=30") != std::string::npos);
    assert(text.str().find("mean_fps=50") != std::string::npos);
    assert(text.str().find("minimized_frames=1") != std::string::npos);
    std::ifstream csv(path);
    unsigned lines = 0;
    for (std::string line; std::getline(csv, line);)
        ++lines;
    assert(lines == 5);
    PerformanceCapture disabled("", 0, 1);
    assert(!disabled.Frame(10, 0, {}, 0, 0, 0, false, false, 0, 0, false));
    // Files are closed on scope exit; Windows disallows removing open files.
}
