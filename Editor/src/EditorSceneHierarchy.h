#pragma once

#include <PlagC/Editor/EditorSession.h>
#include <array>
#include <cstdint>
#include <unordered_set>

namespace PlagC {
class Engine;
}

namespace PlagC::Editor {

class EditorSceneHierarchy {
public:
    void Draw(Engine&, EditorSession&, bool& open);
    bool Locked(std::uint64_t persistentID) const noexcept;
    void Reset() noexcept;

private:
    void ResetForDocument(const SceneDocument&);
    void BeginRename(const AuthoringEntity&);

    const SceneDocument* documentIdentity_ = nullptr;
    std::array<char, 160> search_{};
    std::array<char, 256> rename_{};
    std::unordered_set<std::uint64_t> expanded_;
    std::unordered_set<std::uint64_t> locked_;
    std::uint64_t renamingID_ = 0;
    bool renameFocusPending_ = false;
    bool expansionInitialized_ = false;
};

} // namespace PlagC::Editor
