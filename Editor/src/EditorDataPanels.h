#pragma once

#include <PlagC/Editor/EditorModels.h>
#include <array>
#include <string>

namespace PlagC {
class Engine;
}

namespace PlagC::Editor {

class EditorDataPanels {
public:
    void Initialize(Engine&);
    void RefreshAssets(Engine&);
    void DrawContentBrowser(Engine&);
    void DrawConsole(Engine&);
    void RevealAsset(std::string path);

    std::shared_ptr<const std::vector<AssetBrowserEntry>> ContentSnapshot() const
    {
        return assets_.Snapshot();
    }

private:
    AssetBrowserModel assets_;
    ConsoleFilter consoleFilter_{};
    std::array<char, 256> assetSearch_{};
    std::array<char, 256> consoleSearch_{};
    std::string revealAsset_;
    bool autoScrollConsole_ = true;
};

} // namespace PlagC::Editor