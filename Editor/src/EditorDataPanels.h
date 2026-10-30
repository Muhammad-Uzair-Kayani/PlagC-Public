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
    std::string ConsumeCookedModelActivation();

    std::shared_ptr<const std::vector<AssetBrowserEntry>> ContentSnapshot() const
    {
        return assets_.Snapshot();
    }

private:
    AssetBrowserModel assets_;
    ConsoleFilter consoleFilter_{};
    std::array<char, 256> assetSearch_{};
    std::array<char, 256> consoleSearch_{};
    std::string browserFolder_{"Assets://"};
    std::string selectedAsset_;
    std::string revealAsset_;
    std::string cookedModelActivation_;
    int assetOriginFilter_ = 0;
    int assetTypeFilter_ = 0;
    int assetSort_ = 0;
    bool assetSortAscending_ = true;
    bool assetGridView_ = true;
    bool showInternalAssets_ = false;
    float assetCardWidth_ = 128.0f;
    bool autoScrollConsole_ = true;
};

} // namespace PlagC::Editor
