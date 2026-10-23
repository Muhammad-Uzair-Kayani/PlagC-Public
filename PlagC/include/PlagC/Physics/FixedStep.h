#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace PlagC {
struct FixedStepPlan { unsigned steps=0;double alpha=0,droppedSeconds=0; };
class FixedStepClock {
public:
    explicit FixedStepClock(double hz=60,unsigned maxSteps=4):step_(1/hz),maxSteps_(maxSteps) {
        if(!std::isfinite(hz)||hz<1||hz>240||maxSteps==0||maxSteps>32)throw std::invalid_argument("invalid physics timestep settings");
    }
    FixedStepPlan Advance(double dt) {
        if(!std::isfinite(dt)||dt<0||dt>3600)throw std::invalid_argument("invalid physics frame delta");
        accumulator_+=dt;
        const auto available=std::floor(accumulator_/step_+1e-9);
        FixedStepPlan p;p.steps=static_cast<unsigned>(std::min(available,double(maxSteps_)));
        p.droppedSeconds=(available-p.steps)*step_;
        accumulator_=std::max(0.0,accumulator_-available*step_);
        p.alpha=std::clamp(accumulator_/step_,0.0,1.0);return p;
    }
    double StepSeconds() const {return step_;}
    void Reset(){accumulator_=0;}
private:
    double step_,accumulator_=0;unsigned maxSteps_;
};
}
