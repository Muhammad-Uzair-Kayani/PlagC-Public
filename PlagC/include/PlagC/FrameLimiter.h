#pragma once
#include <chrono>
namespace PlagC {
class FrameLimiter {
public:
    using Clock = std::chrono::steady_clock;
    void SetLocked(bool enabled) { locked_ = enabled; }
    bool IsLocked() const { return locked_; }
    Clock::duration Remaining(Clock::time_point start, Clock::time_point now) const {
        if (!locked_) return Clock::duration::zero();
        const auto budget = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / 60.0));
        const auto left = budget - (now - start);
        return left > Clock::duration::zero() ? left : Clock::duration::zero();
    }
private:
    bool locked_ = true;
};
}
