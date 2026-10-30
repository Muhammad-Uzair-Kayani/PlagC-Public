#include "pgcpch.h"
#include <PlagC/Editor/EditorSession.h>
#include <PlagC/Thread.h>
#include <stdexcept>

namespace PlagC::Editor {

EditorSession::EditorSession(std::unique_ptr<SceneDocument> document)
    : document_(std::move(document)) {
    Thread::RequireMain();
    if (!document_)
        throw std::invalid_argument("Editor session requires a scene document");
}

const SceneDocument& EditorSession::Document() const {
    Thread::RequireMain();
    return *document_;
}

SceneDocument& EditorSession::EditDocument() {
    Thread::RequireMain();
    RequireStopped();
    return *document_;
}

const SceneDocument& EditorSession::ViewDocument() const {
    Thread::RequireMain();
    return play_.Running() ? play_.RuntimeDocument() : *document_;
}

const SceneDocument& EditorSession::RuntimeDocument() const {
    Thread::RequireMain();
    return play_.RuntimeDocument();
}

bool EditorSession::HasPath() const {
    Thread::RequireMain();
    return !path_.empty();
}

const std::filesystem::path& EditorSession::Path() const {
    Thread::RequireMain();
    return path_;
}

bool EditorSession::Dirty() const {
    Thread::RequireMain();
    return document_->Dirty();
}

bool EditorSession::EditingEnabled() const {
    Thread::RequireMain();
    return !play_.Running() && !document_->HasTransientEdit();
}

void EditorSession::NewEmpty(ReplacePolicy policy) {
    Thread::RequireMain();
    RequireReplaceAllowed(policy);

    auto replacement = std::make_unique<SceneDocument>(std::make_unique<World>());
    document_ = std::move(replacement);
    path_.clear();
}

void EditorSession::Open(const std::filesystem::path& path,
                         SceneDocument::ModelResolver resolver,
                         ReplacePolicy policy) {
    Thread::RequireMain();
    RequireReplaceAllowed(policy);
    if (path.empty())
        throw std::invalid_argument("Editor scene path is empty");

    auto replacement = SceneDocument::Load(path, std::move(resolver));
    document_ = std::move(replacement);
    path_ = path;
}

void EditorSession::Save() {
    Thread::RequireMain();
    RequireStopped();
    if (document_->HasTransientEdit())
        throw std::logic_error("Finish or cancel the active editor gesture before saving");
    if (path_.empty())
        throw std::logic_error("Editor scene has no save path");
    document_->Save(path_);
}

void EditorSession::SaveAs(const std::filesystem::path& path) {
    Thread::RequireMain();
    RequireStopped();
    if (document_->HasTransientEdit())
        throw std::logic_error("Finish or cancel the active editor gesture before saving");
    if (path.empty())
        throw std::invalid_argument("Editor scene path is empty");

    document_->Save(path);
    path_ = path;
}

bool EditorSession::CanUndo() const {
    Thread::RequireMain();
    return !play_.Running() && document_->CanUndo();
}

bool EditorSession::CanRedo() const {
    Thread::RequireMain();
    return !play_.Running() && document_->CanRedo();
}

bool EditorSession::Undo() {
    Thread::RequireMain();
    RequireStopped();
    return document_->Undo();
}

bool EditorSession::Redo() {
    Thread::RequireMain();
    RequireStopped();
    return document_->Redo();
}

void EditorSession::StartPlay() {
    Thread::RequireMain();
    if (document_->HasTransientEdit())
        throw std::logic_error("Finish or cancel the active editor gesture before Play");
    play_.Start(*document_);
}

void EditorSession::StopPlay() {
    Thread::RequireMain();
    play_.Stop();
}

void EditorSession::PausePlay() {
    Thread::RequireMain();
    play_.Pause();
}

void EditorSession::ResumePlay() {
    Thread::RequireMain();
    play_.Resume();
}

void EditorSession::TogglePause() {
    Thread::RequireMain();
    play_.TogglePause();
}

void EditorSession::RequestStep() {
    Thread::RequireMain();
    play_.RequestStep();
}

bool EditorSession::ConsumeStep() {
    Thread::RequireMain();
    return play_.ConsumeStep();
}

PlaySession::State EditorSession::PlayState() const {
    Thread::RequireMain();
    return play_.CurrentState();
}

bool EditorSession::Playing() const {
    Thread::RequireMain();
    return play_.Running();
}

bool EditorSession::Paused() const {
    Thread::RequireMain();
    return play_.Paused();
}

std::uint64_t EditorSession::PlayGeneration() const {
    Thread::RequireMain();
    return play_.Generation();
}

const World& EditorSession::ViewWorld() const {
    Thread::RequireMain();
    return ViewDocument().Scene();
}

World& EditorSession::RuntimeWorld() {
    Thread::RequireMain();
    return play_.RuntimeWorld();
}

const World& EditorSession::RuntimeWorld() const {
    Thread::RequireMain();
    return play_.RuntimeWorld();
}

void EditorSession::RequireStopped() const {
    if (play_.Running())
        throw std::logic_error("Editor authoring operation is disabled during Play");
}

void EditorSession::RequireReplaceAllowed(ReplacePolicy policy) const {
    RequireStopped();
    if (document_->HasTransientEdit())
        throw std::logic_error("Finish or cancel the active editor gesture first");
    if (policy == ReplacePolicy::RequireClean && document_->Dirty())
        throw std::logic_error("Editor scene has unsaved changes");
}

} // namespace PlagC::Editor
