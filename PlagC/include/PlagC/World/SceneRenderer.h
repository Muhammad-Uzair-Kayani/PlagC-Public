#pragma once
#include <PlagC/World/World.h>
namespace PlagC
{
// Owns renderer instances, borrows mesh/material handles. Renderer must outlive this bridge.
// Sync/Clear/destruction are main-thread only. World can be destroyed independently.
class SceneRenderer
{
  public:
    explicit SceneRenderer(Renderer &);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer &) = delete;
    SceneRenderer &operator=(const SceneRenderer &) = delete;
    void Sync(const World &);
    void Clear();
    std::size_t Objects() const;
    std::size_t Lights() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
