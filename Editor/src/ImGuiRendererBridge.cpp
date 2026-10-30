#include "pgcpch.h"
#include "ImGuiRendererBridge.h"
#include <PlagC/Renderer/UiTexture.h>
#include <imgui.h>
#include <algorithm>
#include <limits>
#include <span>
#include <stdexcept>

namespace PlagC::Editor {
namespace {
constexpr std::uint64_t TargetBit = std::uint64_t{1} << 63;

UiTextureRef DecodeTexture(ImTextureID id)
{
    const auto value = static_cast<std::uint64_t>(id);
    if ((value & TargetBit) != 0)
        return {UiTextureKind::Target, value & ~TargetBit};
    return {UiTextureKind::Texture, value};
}
} // namespace

std::uint64_t ImGuiRendererBridge::TextureID(TextureHandle handle) noexcept
{
    return handle.value;
}

std::uint64_t ImGuiRendererBridge::TargetID(TargetHandle handle) noexcept
{
    return handle.value ? (TargetBit | handle.value) : 0;
}

void ImGuiRendererBridge::Initialize(Renderer& renderer)
{
    if (!ImGui::GetCurrentContext())
        throw std::logic_error("ImGui renderer bridge requires a context");
    if (fontTexture_)
        throw std::logic_error("ImGui renderer bridge is already initialized");

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0, bytesPerPixel = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);
    if (!pixels || width <= 0 || height <= 0 || bytesPerPixel != 4)
        throw std::runtime_error("ImGui font atlas generation failed");

    // Dear ImGui renders solid shapes through TexUvWhitePixel. If this texel is not
    // opaque white, every widget colour is multiplied by the wrong atlas value.
    const ImVec2 whiteUv = io.Fonts->TexUvWhitePixel;
    const int whiteX = std::clamp(static_cast<int>(whiteUv.x * width), 0, width - 1);
    const int whiteY = std::clamp(static_cast<int>(whiteUv.y * height), 0, height - 1);
    const std::size_t whiteOffset =
        (static_cast<std::size_t>(whiteY) * static_cast<std::size_t>(width) +
         static_cast<std::size_t>(whiteX)) * 4u;
    if (pixels[whiteOffset + 0] != 0xffu || pixels[whiteOffset + 1] != 0xffu ||
        pixels[whiteOffset + 2] != 0xffu || pixels[whiteOffset + 3] != 0xffu)
        throw std::runtime_error("ImGui font atlas white texel is not opaque white");

    const std::size_t bytes = static_cast<std::size_t>(width) *
                              static_cast<std::size_t>(height) * 4u;
    fontTexture_ = CreateUiTexture(renderer,
                                   static_cast<unsigned>(width),
                                   static_cast<unsigned>(height),
                                   std::span<const std::uint8_t>(pixels, bytes),
                                   false);
    io.Fonts->SetTexID(static_cast<ImTextureID>(TextureID(fontTexture_)));
    io.BackendRendererName = "PlagC_Ogre_UI";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
}

void ImGuiRendererBridge::Submit(Renderer& renderer, const ImDrawData* drawData)
{
    if (!drawData || drawData->CmdListsCount == 0 ||
        drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f)
    {
        renderer.SubmitUI({});
        return;
    }

    UiDrawData frame;
    const ImVec2 scale = drawData->FramebufferScale;
    const ImVec2 offset = drawData->DisplayPos;
    frame.width = drawData->DisplaySize.x * scale.x;
    frame.height = drawData->DisplaySize.y * scale.y;

    std::size_t totalVertices = 0;
    std::size_t totalIndices = 0;
    for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex)
    {
        const ImDrawList* list = drawData->CmdLists[listIndex];
        totalVertices += static_cast<std::size_t>(list->VtxBuffer.Size);
        totalIndices += static_cast<std::size_t>(list->IdxBuffer.Size);
    }
    if (totalVertices > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("ImGui frame exceeds 32-bit vertex addressing");

    frame.vertices.reserve(totalVertices);
    frame.indices.reserve(totalIndices);

    for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex)
    {
        const ImDrawList* list = drawData->CmdLists[listIndex];
        const std::uint32_t vertexBase = static_cast<std::uint32_t>(frame.vertices.size());

        for (const ImDrawVert& vertex : list->VtxBuffer)
        {
            UiVertex out;
            out.x = (vertex.pos.x - offset.x) * scale.x;
            out.y = (vertex.pos.y - offset.y) * scale.y;
            out.u = vertex.uv.x;
            out.v = vertex.uv.y;
            out.colour = vertex.col;
            frame.vertices.push_back(out);
        }

        for (const ImDrawCmd& command : list->CmdBuffer)
        {
            if (command.UserCallback)
            {
                if (command.UserCallback != ImDrawCallback_ResetRenderState)
                    command.UserCallback(list, &command);
                continue;
            }
            if (command.ElemCount == 0)
                continue;

            const float left = (command.ClipRect.x - offset.x) * scale.x;
            const float top = (command.ClipRect.y - offset.y) * scale.y;
            const float right = (command.ClipRect.z - offset.x) * scale.x;
            const float bottom = (command.ClipRect.w - offset.y) * scale.y;
            if (right <= 0.0f || bottom <= 0.0f ||
                left >= frame.width || top >= frame.height)
                continue;

            UiDrawCommand out;
            out.firstIndex = static_cast<std::uint32_t>(frame.indices.size());
            out.indexCount = command.ElemCount;
            out.clipLeft = std::clamp(left, 0.0f, frame.width);
            out.clipTop = std::clamp(top, 0.0f, frame.height);
            out.clipRight = std::clamp(right, 0.0f, frame.width);
            out.clipBottom = std::clamp(bottom, 0.0f, frame.height);
            out.texture = DecodeTexture(command.GetTexID());
            if (out.texture.kind == UiTextureKind::Texture &&
                out.texture.value != fontTexture_.value)
                throw std::runtime_error(
                    "ImGui submitted an unknown editor UI texture handle");

            for (unsigned i = 0; i < command.ElemCount; ++i)
            {
                const std::size_t sourceIndex = static_cast<std::size_t>(command.IdxOffset) + i;
                if (sourceIndex >= static_cast<std::size_t>(list->IdxBuffer.Size))
                    throw std::runtime_error("ImGui produced an invalid index range");
                const std::uint64_t globalIndex =
                    static_cast<std::uint64_t>(vertexBase) + command.VtxOffset +
                    static_cast<std::uint64_t>(list->IdxBuffer[static_cast<int>(sourceIndex)]);
                if (globalIndex >= frame.vertices.size())
                    throw std::runtime_error("ImGui produced an invalid vertex index");
                frame.indices.push_back(static_cast<std::uint32_t>(globalIndex));
            }
            frame.commands.push_back(out);
        }
    }

    renderer.SubmitUI(std::move(frame));
}

void ImGuiRendererBridge::Shutdown(Renderer& renderer)
{
    renderer.ClearUI();
    if (fontTexture_)
    {
        DestroyUiTexture(renderer, fontTexture_);
        fontTexture_ = {};
    }
    if (ImGui::GetCurrentContext())
    {
        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererName = nullptr;
        io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    }
}

} // namespace PlagC::Editor
