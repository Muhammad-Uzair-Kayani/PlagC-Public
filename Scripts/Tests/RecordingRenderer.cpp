// CPU-only test backend. It records state/validates handle lifetimes; it is NOT Ogre
// and cannot measure GPU/render-driver costs or verify pixels.
#include <PlagC/Renderer/Renderer.h>
#include <unordered_map>
#include <stdexcept>
namespace PlagC {
namespace { std::uint64_t next=1; Transform lastTransform; }
Transform RecordedLastTransform(){return lastTransform;}
struct Renderer::Impl {
    struct Object { MeshHandle mesh;MaterialHandle material;Transform transform;bool visible=true,shadow=true;std::uint8_t queue=10; };
    std::unordered_map<std::uint64_t,MeshData> meshes;
    std::unordered_map<std::uint64_t,MaterialDesc> materials;
    std::unordered_map<std::uint64_t,Object> objects;
    std::unordered_map<std::uint64_t,LightDesc> lights;
    CameraDesc camera;
};
Renderer::Renderer(Window*,const RendererConfig&):impl_(std::make_unique<Impl>()) {}
Renderer::~Renderer()=default;
MeshHandle Renderer::CreateMesh(const MeshData& d){MeshHandle h{next++};impl_->meshes.emplace(h.value,d);return h;}
void Renderer::DestroyMesh(MeshHandle h){for(auto& [id,o]:impl_->objects)if(o.mesh.value==h.value)throw std::runtime_error("mesh in use");if(!impl_->meshes.erase(h.value))throw std::runtime_error("stale mesh");}
MaterialHandle Renderer::CreateMaterial(const MaterialDesc& d){MaterialHandle h{next++};impl_->materials.emplace(h.value,d);return h;}
void Renderer::DestroyMaterial(MaterialHandle h){for(auto& [id,o]:impl_->objects)if(o.material.value==h.value)throw std::runtime_error("material in use");if(!impl_->materials.erase(h.value))throw std::runtime_error("stale material");}
ObjectHandle Renderer::CreateObject(MeshHandle mesh,MaterialHandle material,const Transform& t){
    impl_->meshes.at(mesh.value);impl_->materials.at(material.value);ObjectHandle h{next++};impl_->objects.emplace(h.value,Impl::Object{mesh,material,t});return h;
}
void Renderer::DestroyObject(ObjectHandle h){if(!impl_->objects.erase(h.value))throw std::runtime_error("stale object");}
void Renderer::SetTransform(ObjectHandle h,const Transform& t){impl_->objects.at(h.value).transform=t;lastTransform=t;}
void Renderer::SetVisible(ObjectHandle h,bool b){impl_->objects.at(h.value).visible=b;}
void Renderer::SetCastShadows(ObjectHandle h,bool b){impl_->objects.at(h.value).shadow=b;}
void Renderer::SetRenderQueue(ObjectHandle h,std::uint8_t q){impl_->objects.at(h.value).queue=q;}
LightHandle Renderer::CreateLight(const LightDesc& d){LightHandle h{next++};impl_->lights.emplace(h.value,d);return h;}
void Renderer::DestroyLight(LightHandle h){if(!impl_->lights.erase(h.value))throw std::runtime_error("stale light");}
void Renderer::UpdateLight(LightHandle h,const LightDesc& d){impl_->lights.at(h.value)=d;}
void Renderer::SetCamera(const CameraDesc& c){impl_->camera=c;}
RenderStats Renderer::Statistics() const {RenderStats s;s.objects=impl_->objects.size();s.meshes=impl_->meshes.size();s.materials=impl_->materials.size();s.lights=impl_->lights.size();return s;}
}
