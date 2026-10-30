#pragma once

#include <PlagC/Editor/EditorModels.h>
#include <array>
#include <string>
#include <vector>

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

    std::string ConsumeCookedModelActivation()
    {
        std::string result = std::move(cookedModelActivation_);
        cookedModelActivation_.clear();
        return result;
    }

private:
    enum class ContentSort
    {
        NameAscending,
        NameDescending,
        Largest,
        Smallest,
        Type
    };

    void NavigateContent(std::string folder, bool recordHistory = true);
    void QueueCookedModelActivation(std::string path);

    AssetBrowserModel assets_;
    ConsoleFilter consoleFilter_{};
    std::array<char, 256> assetSearch_{};
    std::array<char, 256> consoleSearch_{};
    std::string revealAsset_;
    std::string selectedAsset_;
    std::string contentFolder_;
    std::string cookedModelActivation_;
    std::vector<std::string> contentHistory_{std::string{}};
    std::size_t contentHistoryIndex_ = 0;
    int contentKindFilter_ = -1;
    ContentSort contentSort_ = ContentSort::NameAscending;
    bool contentGridView_ = true;
    bool contentShowInternal_ = false;
    bool autoScrollConsole_ = true;
};

} // namespace PlagC::Editor
