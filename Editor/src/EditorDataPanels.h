#pragma once

#include "EditorThumbnailCache.h"
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
    void Shutdown(Engine&);
    void RefreshAssets(Engine&);
    void DrawContentBrowser(Engine&);
    void DrawAssetPreview(Engine&);
    void DrawConsole(Engine&);
    void RevealAsset(std::string path);
    std::string ConsumeCookedModelActivation();

    std::shared_ptr<const std::vector<AssetBrowserEntry>> ContentSnapshot() const
    {
        return assets_.Snapshot();
    }

protected:
    AssetBrowserModel assets_;
    EditorThumbnailCache thumbnails_;
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
    float assetCardWidth_ = 144.0f;
    bool autoScrollConsole_ = true;
};

// Composite used by EditorLayer. The M13 browser remains the owner of navigation/cards;
// M14 appends preview output without duplicating its asset model or filesystem workflow.
class EditorPreviewDataPanels final : public EditorDataPanels {
public:
    ~EditorPreviewDataPanels();
    void Initialize(Engine&);
    void DrawContentBrowser(Engine&);
private:
    Engine* engine_ = nullptr;
};

} // namespace PlagC::Editor
