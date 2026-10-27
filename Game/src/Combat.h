#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace Game
{
struct Health
{
    float value = 100;
    bool Alive() const
    {
        return value > 0;
    }
    void Damage(float amount)
    {
        if (!std::isfinite(amount) || amount < 0)
            throw std::invalid_argument("invalid damage");
        value = std::max(0.f, value - amount);
    }
};
struct Weapon
{
    static constexpr double ReloadDuration = 2.1;
    unsigned rounds = 30, reserve = 150;
    double cooldown = 0, reload = 0;
    bool Reload()
    {
        if (reload > 0 || rounds == 30 || !reserve)
            return false;
        reload = ReloadDuration;
        return true;
    }
    void Tick(double dt)
    {
        if (!std::isfinite(dt) || dt < 0)
            throw std::invalid_argument("invalid weapon timestep");
        cooldown = std::max(0., cooldown - dt);
        if (reload > 0)
        {
            reload = std::max(0., reload - dt);
            if (reload == 0)
            {
                auto n = std::min(30 - rounds, reserve);
                rounds += n;
                reserve -= n;
            }
        }
    }
    bool Fire()
    {
        if (reload > 0 || cooldown > 0 || !rounds)
            return false;
        --rounds;
        cooldown = .12;
        return true;
    }
};
} // namespace Game
