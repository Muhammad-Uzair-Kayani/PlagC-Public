#include "pgcpch.h"
#include <PlagC/Renderer/Renderer.h>
#include <PlagC/Thread.h>
#include "UiSubmissionInternal.h"
#include <cmath>
#include <unordered_map>

namespace PlagC {
namespace {
std::unordered_map<const Renderer*, UiDrawData> gPendingUiFrames;

void ValidateUI(const UiDrawData& data)
{
    if (!std::isfinite(data.width) || !std::isfinite(data.height) ||
        data.width < 0.0f || data.height < 0.0f)
        throw std::invalid_argument("invalid UI display size");

    for (const auto& vertex : data.vertices)
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
            !std::isfinite(vertex.u) || !std::isfinite(vertex.v))
            throw std::invalid_argument("nonfinite UI vertex");

    for (const auto index : data.indices)
        if (index >= data.vertices.size())
            throw std::invalid_argument("UI index out of range");

    for (const auto& command : data.commands)
    {
        const std::uint64_t end = static_cast<std::uint64_t>(command.firstIndex) +
                                  static_cast<std::uint64_t>(command.indexCount);
        if (end > data.indices.size())
            throw std::invalid_argument("UI draw command index range is invalid");
        if (!std::isfinite(command.clipLeft) || !std::isfinite(command.clipTop) ||
            !std::isfinite(command.clipRight) || !std::isfinite(command.clipBottom) ||
            command.clipRight < command.clipLeft || command.clipBottom < command.clipTop)
            throw std::invalid_argument("invalid UI clip rectangle");
        if (command.texture.value == 0)
            throw std::invalid_argument("UI draw command has no texture");
    }
}
} // namespace

void Renderer::SubmitUI(UiDrawData data)
{
    Thread::RequireMain();
    ValidateUI(data);
    if (backend_ != RendererBackend::Null)
        Detail::ValidateOgreUITextures(data);
    gPendingUiFrames[this] = std::move(data);
    if (backend_ != RendererBackend::Null)
        Detail::RegisterOgreUIRenderer(this);
}

void Renderer::ClearUI()
{
    Thread::RequireMain();
    Detail::UnregisterOgreUIRenderer(this);
    Detail::RemoveUIFrame(this);
}

namespace Detail {
const UiDrawData* PendingUIFrame(const Renderer* renderer) noexcept
{
    const auto it = gPendingUiFrames.find(renderer);
    return it == gPendingUiFrames.end() ? nullptr : &it->second;
}

void RemoveUIFrame(const Renderer* renderer) noexcept
{
    gPendingUiFrames.erase(renderer);
}
} // namespace Detail
} // namespace PlagC
