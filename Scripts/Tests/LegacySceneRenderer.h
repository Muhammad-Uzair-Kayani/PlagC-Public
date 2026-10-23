#pragma once
#include <PlagC/World/World.h>
namespace PlagC {
class LegacySceneRenderer {
public:
    explicit LegacySceneRenderer(Renderer&);
    ~LegacySceneRenderer();
    void Sync(const World&);
    void Clear();
    std::size_t Objects() const;
    std::size_t Lights() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
