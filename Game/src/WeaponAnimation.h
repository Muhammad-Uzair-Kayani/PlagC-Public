#pragma once
#include <algorithm>
#include <cmath>
namespace Game
{
// Weapon intent is persistent; a successful shot is an event, never a locomotion state.
struct WeaponAnimation
{
    double hold = 0, recoilAge = 1;
    float weight = 0;
    bool ready = false;
    void Update(double dt, bool aim, bool fireHeld, bool shot, bool blocked)
    {
        hold = std::max(0., hold - dt);
        recoilAge += dt;
        if (shot && !blocked)
        {
            hold = .28;
            recoilAge = 0;
        }
        ready = !blocked && (aim || fireHeld || hold > 0);
        if (blocked)
        {
            hold = 0;
            recoilAge = 1;
        }
        const float step = float(dt) / (ready ? .10f : .18f);
        weight = ready ? std::min(1.f, weight + step) : std::max(0.f, weight - step);
    }
    float Recoil() const
    {
        // Smooth attack and recovery, synchronized to accepted shots, not input repeats.
        const float t = float(recoilAge);
        if (t >= .12f)
            return 0;
        return -.035f * std::sin(3.14159265f * std::clamp(t / .12f, 0.f, 1.f));
    }
};
} // namespace Game
