#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif
namespace PlagC
{
// Opt-in owner-thread CSV capture. CPU wall time includes blocking/driver waits, never GPU time.
class PerformanceCapture
{
    std::ofstream csv_, summary_;
    unsigned warmup_ = 0;
    double limitMs_ = 0, elapsedMs_ = 0, memoryAgeMs_ = 1000, residentMiB_ = -1;
    std::size_t frame_ = 0, minimized_ = 0;
    std::vector<double> samples_;
    bool finished_ = false, observedFirst_ = false;

    static double ResidentMiB()
    {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS counters{};
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
            return double(counters.WorkingSetSize) / (1024 * 1024);
#elif defined(__linux__)
        std::ifstream stat("/proc/self/statm");
        unsigned long total = 0, resident = 0;
        auto page = sysconf(_SC_PAGESIZE);
        if (stat >> total >> resident && page > 0)
            return double(resident) * double(page) / (1024 * 1024);
#endif
        return -1; // Unsupported/unavailable, never reported as zero usage.
    }

  public:
    PerformanceCapture(const std::string &path, unsigned warmup, double seconds)
        : warmup_(warmup), limitMs_(seconds * 1000)
    {
        if (path.empty())
            return;
        if (!std::isfinite(seconds) || seconds <= 0)
            throw std::invalid_argument("invalid capture duration");
        auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);
        csv_.open(path, std::ios::trunc);
        summary_.open(path + ".summary.txt", std::ios::trunc);
        if (!csv_ || !summary_)
            throw std::runtime_error("Cannot open performance capture: " + path);
        csv_ << std::fixed << std::setprecision(4)
             << "frame,elapsed_ms,frame_ms,update_cpu_ms,render_cpu_ms,physics_cpu_ms,"
                "animation_cpu_ms,sync_cpu_ms,draw_calls,triangles,objects,meshes,materials,"
                "textures,lights,render_instances,resident_mib,fps_locked,vsync,width,height,"
                "minimized,gpu_ms\n";
    }
    void Startup(double engineMs, double sceneMs, unsigned workers, unsigned physicsWorkers,
                 const std::string &scheduler, bool synthetic)
    {
        if (!csv_.is_open())
            return;
#ifdef PC_DIST
        constexpr auto config = "Dist";
#elif defined(PC_DEBUG)
        constexpr auto config = "Debug";
#else
        constexpr auto config = "Release";
#endif
        summary_ << "configuration=" << config << "\nengine_init_ms=" << engineMs
                 << "\nscene_start_ms=" << sceneMs << "\nworkers=" << workers
                 << "\nphysics_workers=" << physicsWorkers << "\nphysics_scheduler=" << scheduler
                 << "\nsynthetic_backend=" << synthetic << "\nwarmup_frames=" << warmup_
                 << "\ngpu_ms=unavailable\n";
    }
    // frameMs is the complete current iteration, including poll, update, render and pacing.
    bool Frame(double frameMs, double updateMs, const RenderStats &s, double physicsMs,
               double animationMs, double syncMs, bool locked, bool vsync, int width, int height,
               bool minimized)
    {
        if (!csv_.is_open() || finished_)
            return false;
        if (!std::isfinite(frameMs) || frameMs < 0)
            throw std::invalid_argument("invalid capture frame time");
        if (!observedFirst_)
        {
            summary_ << "first_iteration_ms=" << frameMs << '\n';
            observedFirst_ = true;
        }
        if (warmup_)
        {
            --warmup_;
            return false;
        }
        elapsedMs_ += frameMs;
        memoryAgeMs_ += frameMs;
        if (memoryAgeMs_ >= 1000)
        {
            residentMiB_ = ResidentMiB();
            memoryAgeMs_ = 0;
        }
        if (minimized)
            ++minimized_;
        else
            samples_.push_back(frameMs);
        csv_ << ++frame_ << ',' << elapsedMs_ << ',' << frameMs << ',' << updateMs << ','
             << s.cpuMilliseconds << ',' << physicsMs << ',' << animationMs << ',' << syncMs << ','
             << s.drawCalls << ',' << s.triangles << ',' << s.objects << ',' << s.meshes << ','
             << s.materials << ',' << s.textures << ',' << s.lights << ',' << s.instances << ','
             << residentMiB_ << ',' << locked << ',' << vsync << ',' << width << ',' << height
             << ',' << minimized << ",NA\n";
        return elapsedMs_ >= limitMs_;
    }
    void Finish()
    {
        if (!csv_.is_open() || finished_)
            return;
        finished_ = true;
        double sum = 0;
        for (auto ms : samples_)
            sum += ms;
        std::sort(samples_.begin(), samples_.end());
        auto percentile = [&](double p) {
            return samples_.empty() ? 0.
                                    : samples_[std::size_t(std::ceil(p * samples_.size())) - 1];
        };
        summary_ << "recorded_frames=" << frame_ << "\nminimized_frames=" << minimized_
                 << "\nvalid_frames=" << samples_.size()
                 << "\nmean_fps=" << (sum > 0 ? samples_.size() * 1000 / sum : 0)
                 << "\nframe_p50_ms=" << percentile(.50) << "\nframe_p95_ms=" << percentile(.95)
                 << "\nframe_p99_ms=" << percentile(.99)
                 << "\nframe_max_ms=" << (samples_.empty() ? 0 : samples_.back()) << '\n';
        csv_.flush();
        summary_.flush();
        if (!csv_ || !summary_)
            throw std::runtime_error("Performance capture write failed");
    }
};
} // namespace PlagC
