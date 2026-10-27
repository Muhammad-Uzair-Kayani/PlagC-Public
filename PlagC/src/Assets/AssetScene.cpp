#include "pgcpch.h"
#include <PlagC/Assets/AssetScene.h>
#include <PlagC/Profiler.h>
#include "../World/WorldInternal.h"
namespace PlagC
{
namespace
{
using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now() - t).count(); }
}
struct AssetScene::Impl
{
    Renderer &renderer; std::uint64_t world = 0, frame = 0; AssetSceneStats stats;
    struct Resources
    {
        Renderer &renderer; AssetHandle<ModelAsset> asset; std::vector<TextureHandle> textures;
        std::vector<MaterialHandle> materials; std::vector<MeshHandle> meshes;
        Resources(Renderer &r, AssetHandle<ModelAsset> a) : renderer(r), asset(std::move(a)) {}
        ~Resources() { Thread::RequireMain(); for (auto h : meshes) renderer.DestroyMesh(h); for (auto h : materials) renderer.DestroyMaterial(h); for (auto h : textures) renderer.DestroyTexture(h); }
        void Build()
        {
            Validate(*asset); std::unordered_map<std::uint64_t, TextureHandle> cache;
            auto texture = [&](int index, bool srgb) { if (index < 0) return TextureHandle{}; auto key = std::uint64_t(index) * 2 + srgb; if (auto found = cache.find(key); found != cache.end()) return found->second; auto &t = asset->textures.at(index); auto h = renderer.CreateTexture(t.width, t.height, t.rgba, srgb, true); textures.push_back(h); cache.emplace(key, h); return h; };
            for (auto &m : asset->materials) { MaterialDesc d; d.colour=m.colour; d.roughness=m.roughness; d.metallic=m.metallic; d.emissive=m.emissive; d.normalScale=m.normalScale; d.baseColourTexture=texture(m.baseColourTexture,true); d.normalTexture=texture(m.normalTexture,false); d.metallicTexture=texture(m.metallicTexture,false); d.roughnessTexture=texture(m.roughnessTexture,false); d.occlusionTexture=texture(m.occlusionTexture,false); d.emissiveTexture=texture(m.emissiveTexture,true); materials.push_back(renderer.CreateMaterial(d)); }
            Animator rest(asset); for (unsigned i=0;i<asset->meshes.size();++i) meshes.push_back(renderer.CreateMesh(rest.Deform(i)));
        }
    };
    struct Instance
    {
        Renderer &renderer; std::shared_ptr<Resources> resources; std::vector<ObjectHandle> objects; std::vector<MeshHandle> deformed; std::unique_ptr<Animator> animator; std::uint64_t seen=0, transform=0; bool visible=true;
        explicit Instance(Renderer &r,std::shared_ptr<Resources>a):renderer(r),resources(std::move(a)){}
        ~Instance(){Thread::RequireMain();for(auto h:objects)renderer.DestroyObject(h);for(auto h:deformed)renderer.DestroyMesh(h);}
        void Build(bool animate){if(animate)animator=std::make_unique<Animator>(resources->asset);for(unsigned i=0;i<resources->meshes.size();++i){auto mesh=resources->meshes[i];if(animate){mesh=renderer.CreateMesh(animator->Deform(i),true);deformed.push_back(mesh);}auto material=resources->materials.at(resources->asset->meshes[i].material);objects.push_back(renderer.CreateObject(mesh,material));}}
    };
    std::unordered_map<const ModelAsset*,std::weak_ptr<Resources>> resources; std::unordered_map<std::uint64_t,std::unique_ptr<Instance>> instances;
    explicit Impl(Renderer&r):renderer(r){}
    std::shared_ptr<Resources> Acquire(AssetHandle<ModelAsset> asset){auto key=asset.Shared().get();auto &weak=resources[key];if(auto existing=weak.lock())return existing;auto result=std::make_shared<Resources>(renderer,asset);result->Build();weak=result;return result;}
};
AssetScene::AssetScene(Renderer&r):impl_(std::make_unique<Impl>(r)){Thread::RequireMain();}
AssetScene::~AssetScene(){Clear();}
void AssetScene::Clear(){Thread::RequireMain();impl_->instances.clear();impl_->resources.clear();impl_->world=0;impl_->stats={};}
std::optional<Vec3> AssetScene::AnimatedNodePoint(const World&world,Entity entity,const std::string&name,Vec3 local)const
{
    Thread::RequireMain();if(impl_->world!=world.impl_->identity||!world.Alive(entity))return{};auto it=impl_->instances.find(entity.value);if(it==impl_->instances.end()||!it->second->animator)return{};const auto&joints=it->second->resources->asset->skeleton.joints;
    for(std::size_t i=0;i<joints.size();++i)if(joints[i].name==name){auto point=TransformPoint(it->second->animator->Pose()[i],local);const auto t=world.ReadTransform(entity);point={point.x*t.scale.x,point.y*t.scale.y,point.z*t.scale.z};constexpr float rad=3.14159265358979323846f/180;auto rotate=[](float&a,float&b,float angle){float x=a;a=x*std::cos(angle)-b*std::sin(angle);b=x*std::sin(angle)+b*std::cos(angle);};rotate(point.x,point.y,t.rotationDegrees.z*rad);rotate(point.y,point.z,t.rotationDegrees.x*rad);rotate(point.z,point.x,t.rotationDegrees.y*rad);return Vec3{point.x+t.position.x,point.y+t.position.y,point.z+t.position.z};}return{};
}
void AssetScene::Update(World&world,double dt)
{
    Thread::RequireMain();if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("invalid animation delta");auto&p=*impl_;auto&w=*world.impl_;if(p.world!=w.identity){Clear();p.world=w.identity;}++p.frame;p.stats={};
    for(auto raw:w.registry.view<ModelComponent,Transform,Entity>())
    {
        auto entity=w.registry.get<Entity>(raw);auto&model=w.registry.get<ModelComponent>(raw);auto*animation=w.registry.try_get<AnimatorComponent>(raw);auto it=p.instances.find(entity.value);
        if(it==p.instances.end()||it->second->resources->asset.Shared()!=model.asset.Shared()||bool(it->second->animator)!=bool(animation)){auto replacement=std::make_unique<Impl::Instance>(p.renderer,p.Acquire(model.asset));replacement->Build(animation!=nullptr);p.instances[entity.value]=std::move(replacement);it=p.instances.find(entity.value);}
        auto&instance=*it->second;instance.seen=p.frame;auto version=w.registry.get<WorldChanges>(raw).transform;if(instance.transform!=version){for(auto h:instance.objects)p.renderer.SetTransform(h,w.registry.get<Transform>(raw));instance.transform=version;}if(instance.visible!=model.visible){for(auto h:instance.objects)p.renderer.SetVisible(h,model.visible);instance.visible=model.visible;}
        if(animation){auto start=Clock::now();instance.animator->Update(*animation,dt);const double pose=Ms(start);p.stats.poseMs+=pose;p.stats.animationMs+=pose;for(unsigned i=0;i<instance.deformed.size();++i){start=Clock::now();auto data=instance.animator->Deform(i);const double skin=Ms(start);p.stats.skinningMs+=skin;p.stats.animationMs+=skin;start=Clock::now();p.renderer.UpdateMesh(instance.deformed[i],data.vertices);p.stats.uploadMs+=Ms(start);}++p.stats.animated;}
    }
    std::erase_if(p.instances,[&](const auto&item){return item.second->seen!=p.frame;});std::erase_if(p.resources,[](const auto&item){return item.second.expired();});p.stats.instances=p.instances.size();p.stats.models=p.resources.size();
    auto &detail=DetailedTimings().animation;detail.poseMs=p.stats.poseMs;detail.skinningMs=p.stats.skinningMs;detail.uploadMs=p.stats.uploadMs;
}
AssetSceneStats AssetScene::Statistics()const{Thread::RequireMain();return impl_->stats;}
} // namespace PlagC
