#pragma once

#include <PlagC/Editor/PlaySession.h>
#include <filesystem>
#include <memory>

namespace PlagC::Editor {

enum class ReplacePolicy {
    RequireClean,
    DiscardChanges
};

class EditorSession {
public:
    explicit EditorSession(std::unique_ptr<SceneDocument>);
    ~EditorSession() = default;

    EditorSession(const EditorSession&) = delete;
    EditorSession& operator=(const EditorSession&) = delete;

    const SceneDocument& Document() const;
    SceneDocument& EditDocument();
    const SceneDocument& ViewDocument() const;
    const SceneDocument& RuntimeDocument() const;

    bool HasPath() const;
    const std::filesystem::path& Path() const;
    bool Dirty() const;
    bool EditingEnabled() const;

    void NewEmpty(ReplacePolicy = ReplacePolicy::RequireClean);
    void Open(const std::filesystem::path&,
              SceneDocument::ModelResolver = {},
              ReplacePolicy = ReplacePolicy::RequireClean);
    void Save();
    void SaveAs(const std::filesystem::path&);

    bool CanUndo() const;
    bool CanRedo() const;
    bool Undo();
    bool Redo();

    void StartPlay();
    void StopPlay();
    void PausePlay();
    void ResumePlay();
    void TogglePause();
    void RequestStep();
    bool ConsumeStep();

    PlaySession::State PlayState() const;
    bool Playing() const;
    bool Paused() const;
    std::uint64_t PlayGeneration() const;

    const World& ViewWorld() const;
    // Mutable view reserved for editor-owned runtime/render systems. UI authoring must
    // continue through EditDocument() so commands, dirty state and undo/redo are preserved.
    World& SystemWorld();
    World& RuntimeWorld();
    const World& RuntimeWorld() const;

private:
    void RequireStopped() const;
    void RequireReplaceAllowed(ReplacePolicy) const;

    std::unique_ptr<SceneDocument> document_;
    std::filesystem::path path_;
    PlaySession play_;
};

} // namespace PlagC::Editor
