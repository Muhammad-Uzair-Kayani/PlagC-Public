#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <fstream>

int main(int argc, char** argv) {
    using namespace PlagC;
    assert(argc == 2);
    Thread::InitializeMain();
    const std::filesystem::path root = argv[1];
    auto file = root/"sample.plasset";
    TextureAsset texture;
    texture.name="sample"; texture.width=texture.height=1; texture.rgba={1,2,3,255};
    SaveAsset(texture,file);
    JobSystem jobs(2);
    AssetManager assets(jobs,root);
    const auto retained=assets.Load<TextureAsset>("sample.plasset");
    for (bool missing : {false,true}) {
        if (missing) std::filesystem::remove(file);
        else std::ofstream(file,std::ios::binary|std::ios::trunc)<<"bad payload";
        bool failed=false;
        try { assets.ReloadAndWait("sample.plasset"); }
        catch(const std::runtime_error& e) { failed=std::string(e.what()).find("sample.plasset")!=std::string::npos; }
        assert(failed && assets.Registry().front().state==AssetState::Failed);
        assert(assets.Get<TextureAsset>("sample.plasset").Shared()==retained.Shared());
    }
    texture.rgba[0]=42; SaveAsset(texture,file);
    assets.ReloadAndWait("sample.plasset");
    auto replacement=assets.Get<TextureAsset>("sample.plasset");
    assert(assets.Registry().front().state==AssetState::Ready && replacement->rgba[0]==42);
    assert(replacement->metadata.generation!=retained->metadata.generation && retained->rgba[0]==1);
}
