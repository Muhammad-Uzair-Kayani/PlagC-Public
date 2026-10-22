#include <PlagC/Jobs.h>
#include <PlagC/Thread.h>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace PlagC {
static_assert(std::atomic<void*>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
thread_local JobSystem* JobSystem::current_ = nullptr;
thread_local unsigned JobSystem::workerIndex_ = 0;
thread_local JobGroup* JobSystem::executingGroup_ = nullptr;
bool JobSystem::Deque::Push(Job* job) {
    const auto b = bottom.load();
    if (b - top.load() >= static_cast<std::int64_t>(QueueCapacity)) return false;
    slots[static_cast<std::size_t>(b) % QueueCapacity].store(job);
    bottom.store(b + 1);
    return true;
}
JobSystem::Job* JobSystem::Deque::Pop() {
    auto b = bottom.load() - 1;
    bottom.store(b);
    auto t = top.load();
    if (t > b) { bottom.store(b + 1); return nullptr; }
    Job* job = slots[static_cast<std::size_t>(b) % QueueCapacity].load();
    if (t == b) {
        if (!top.compare_exchange_strong(t, t + 1)) job = nullptr;
        bottom.store(b + 1);
    }
    return job;
}
JobSystem::Job* JobSystem::Deque::Steal() {
    auto t = top.load();
    if (t >= bottom.load()) return nullptr;
    Job* job = slots[static_cast<std::size_t>(t) % QueueCapacity].load();
    return top.compare_exchange_strong(t, t + 1) ? job : nullptr;
}
JobSystem::JobSystem(unsigned count) : jobs_(std::make_unique<Job[]>(Capacity)), owner_(std::this_thread::get_id()) {
    if (!count) count = std::min(32u, std::max(1u, std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1));
    if (count > 256) throw std::invalid_argument("worker count exceeds 256");
    for (unsigned i = 0; i < count; ++i) workers_.push_back(std::make_unique<Worker>());
    try {
        for (unsigned i = 0; i < count; ++i) workers_[i]->thread = std::thread([this,i]{ Run(i); });
    } catch (...) {
        stopping_.store(true); Signal();
        for (auto& w : workers_) if (w->thread.joinable()) w->thread.join();
        throw;
    }
}
JobSystem::~JobSystem() { Shutdown(); }
void JobSystem::Signal() { epoch_.fetch_add(1); epoch_.notify_all(); }
SubmitResult JobSystem::TrySubmit(JobGroup& group, JobFunction function, void* context) {
    if (!function) throw std::invalid_argument("null job function");
    auto gate = admission_.load();
    do { if (gate & Closing) return SubmitResult::Stopped; }
    while (!admission_.compare_exchange_weak(gate, gate + 1));
    Job* job = nullptr;
    const auto start = poolCursor_.fetch_add(1);
    for (std::size_t i = 0; i < Capacity; ++i) {
        bool unused = false;
        auto& candidate = jobs_[(start+i) % Capacity];
        if (candidate.used.compare_exchange_strong(unused, true)) { job = &candidate; break; }
    }
    if (!job) { admission_.fetch_sub(1); admission_.notify_all(); return SubmitResult::Full; }
    job->function = function; job->context = context; job->group = &group;
    group.pending.fetch_add(1); outstanding_.fetch_add(1);
    bool published = current_ == this && workers_[workerIndex_]->queue.Push(job);
    if (!published) {
        const auto pos = inboxCursor_.fetch_add(1);
        for (std::size_t i = 0; i < Capacity; ++i) {
            Job* empty = nullptr;
            if (incoming_[(pos+i) % Capacity].compare_exchange_strong(empty, job)) { published = true; break; }
        }
    }
    if (!published) {
        group.pending.fetch_sub(1);
        outstanding_.fetch_sub(1); outstanding_.notify_all(); job->used.store(false); Signal();
    } else { submitted_.fetch_add(1); Signal(); }
    admission_.fetch_sub(1); admission_.notify_all();
    return published ? SubmitResult::Accepted : SubmitResult::Full;
}
JobSystem::Job* JobSystem::Find(unsigned index) {
    if (auto* j = workers_[index]->queue.Pop()) return j;
    for (unsigned offset = 1; offset < workers_.size(); ++offset) {
        if (auto* j = workers_[(index+offset) % workers_.size()]->queue.Steal()) { stolen_.fetch_add(1); return j; }
    }
    const auto start = inboxCursor_.fetch_add(1);
    for (std::size_t i = 0; i < Capacity; ++i) {
        if (auto* j = incoming_[(start+i) % Capacity].exchange(nullptr)) return j;
    }
    return nullptr;
}
void JobSystem::Execute(Job* job) {
    auto* group = job->group;
    auto* previous = executingGroup_; executingGroup_ = group;
    try { job->function(job->context); }
    catch (...) { group->failed.fetch_add(1); failed_.fetch_add(1); }
    executingGroup_ = previous;
    executed_.fetch_add(1);
    job->used.store(false);
    group->pending.fetch_sub(1);
    outstanding_.fetch_sub(1); outstanding_.notify_all();
    Signal();
}
void JobSystem::Run(unsigned index) {
    Thread::Register("Worker-" + std::to_string(index), true);
    current_ = this; workerIndex_ = index;
    while (true) {
        const auto observed = epoch_.load();
        if (auto* job = Find(index)) { Execute(job); continue; }
        if (stopping_.load()) break;
        epoch_.wait(observed);
    }
    current_ = nullptr;
}
void JobSystem::Wait(JobGroup& group) {
    if (current_ == this) {
        if (executingGroup_ == &group) throw std::logic_error("job cannot wait for its own completion group");
        while (group.pending.load()) {
            const auto observed = epoch_.load();
            if (auto* job = Find(workerIndex_)) Execute(job);
            else if (group.pending.load()) epoch_.wait(observed);
        }
    } else {
        while (true) {
            const auto observed = epoch_.load();
            if (!group.pending.load()) break;
            epoch_.wait(observed);
        }
    }
}
void JobSystem::Shutdown() {
    if (std::this_thread::get_id() != owner_) throw std::logic_error("Shutdown must run on scheduler owner thread");
    if (joined_) return;
    auto gate = admission_.fetch_or(Closing) | Closing;
    while (gate != Closing) { admission_.wait(gate); gate = admission_.load(); }
    auto pending = outstanding_.load();
    while (pending) { outstanding_.wait(pending); pending = outstanding_.load(); }
    stopping_.store(true); Signal();
    for (auto& w : workers_) if (w->thread.joinable()) w->thread.join();
    joined_ = true;
}
JobStats JobSystem::Stats() const { return {submitted_.load(), executed_.load(), stolen_.load(), failed_.load()}; }
}
