#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace PlagC {
// Counters are reusable after Wait. Callers own the group and job contexts until completion.
struct JobGroup {
    std::atomic<std::uint64_t> pending{0}, failed{0};
};
using JobFunction = void (*)(void*);
enum class SubmitResult { Accepted, Full, Stopped };
struct JobStats {
    std::uint64_t submitted = 0, executed = 0, stolen = 0, failed = 0;
    // Cumulative worker execution time and the current accepted-but-not-completed count.
    // Both are telemetry-only atomics; they do not participate in scheduler synchronization.
    std::uint64_t busyNanoseconds = 0, outstanding = 0;
};
class JobSystem {
public:
    explicit JobSystem(unsigned workers = 0);
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    SubmitResult TrySubmit(JobGroup&, JobFunction, void* context);
    void Wait(JobGroup&); // Worker callers help execute work; waiting on your own group is rejected.
    void Shutdown();     // Main/owner thread only. Reject new work, drain accepted work, join.
    unsigned WorkerCount() const { return static_cast<unsigned>(workers_.size()); }
    JobStats Stats() const;
private:
    static constexpr std::size_t Capacity = 4096, QueueCapacity = 1024;
    static constexpr std::uint64_t Closing = std::uint64_t{1} << 63;
    struct Job {
        std::atomic<bool> used{false};
        JobFunction function = nullptr;
        void* context = nullptr;
        JobGroup* group = nullptr;
    };
    // Fixed-capacity Chase-Lev deque. One owner; multiple thieves.
    // All ordering is sequentially consistent initially, including atomic slot access.
    struct Deque {
        std::array<std::atomic<Job*>, QueueCapacity> slots{};
        alignas(64) std::atomic<std::int64_t> top{0};
        alignas(64) std::atomic<std::int64_t> bottom{0};
        bool Push(Job*);
        Job* Pop();
        Job* Steal();
    };
    struct Worker { Deque queue; std::thread thread; };
    std::unique_ptr<Job[]> jobs_;
    // Unordered, bounded MPMC mailbox: each slot independently transfers one pointer.
    std::array<std::atomic<Job*>, Capacity> incoming_{};
    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<std::uint64_t> admission_{0}, outstanding_{0}, epoch_{0};
    std::atomic<std::uint64_t> submitted_{0}, executed_{0}, stolen_{0}, failed_{0}, busyNanoseconds_{0};
    std::atomic<std::size_t> poolCursor_{0}, inboxCursor_{0};
    std::atomic<bool> stopping_{false};
    const std::thread::id owner_;
    bool joined_ = false;
    static thread_local JobSystem* current_;
    static thread_local unsigned workerIndex_;
    static thread_local JobGroup* executingGroup_;
    Job* Find(unsigned);
    void Execute(Job*);
    void Run(unsigned);
    void Signal();
};
}