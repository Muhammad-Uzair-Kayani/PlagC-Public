#pragma once

#include <PlagC/Editor/EditorSession.h>
#include <cstdint>
#include <memory>

namespace PlagC {
class Engine;
}

namespace PlagC::Editor {

class EditorTransformInspector {
public:
    void Draw(Engine&, EditorSession&, const AuthoringEntity&, bool hierarchyLocked);
    void Finish(Engine&, bool commit);
    void Reset() noexcept;

private:
    void Sync(EditorSession&, const AuthoringEntity&);
    void BeginGesture(Engine&, EditorSession&, const AuthoringEntity&);
    void Preview(Engine&, EditorSession&, const AuthoringEntity&);
    void ApplyImmediate(Engine&, EditorSession&, const AuthoringEntity&, const Transform&);

    bool worldSpace_ = false;
    bool uniformScale_ = false;
    std::uint64_t persistentID_ = 0;
    std::uint64_t revision_ = 0;
    Transform value_{};
    std::unique_ptr<SceneDocument::TransformEdit> edit_;
    std::uint64_t editPersistentID_ = 0;
};

} // namespace PlagC::Editor
