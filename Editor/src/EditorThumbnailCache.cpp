#include "pgcpch.h"
#include "EditorThumbnailCache.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/Asset.h>
#include <PlagC/Jobs.h>
#include <PlagC/Renderer/UiTexture.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace PlagC::Editor {
namespace {

constexpr std::size_t MaxCachedThumbnails = 256;
constexpr std::size_t MaxConcurrentThumbnailJobs = 2;
constexpr float Pi = 3.14159265358979323846f;

struct PreviewPixels {
    unsigned width = EditorThumbnailCache::PreviewSize;
    unsigned height = EditorThumbnailCache::PreviewSize;
    std::vector<std::uint8_t> rgba;
    PreviewPixels() : rgba(static_cast<std::size_t>(width) * height * 4u, 255u) {}
};

struct PreviewCamera {
    Vec3 position{2.45f, 1.65f, 2.85f};
    Vec3 target{0.0f, 0.08f, 0.0f};
    float fovDegrees = 34.0f;
};

struct PreviewLight {
    Vec3 direction;
    Vec3 colour;
    float intensity;
};

// Fixed studio preview scene. Model and material thumbnails share this camera/light rig;
// workers generate only RGBA pixels, so OpenGL and Vulkan present identical previews.
constexpr PreviewCamera StudioCamera{};
constexpr std::array<PreviewLight, 3> StudioLights{{
    {{-0.45f, -0.80f, -0.35f}, {1.00f, 0.94f, 0.86f}, 0.95f},
    {{ 0.75f, -0.45f,  0.15f}, {0.55f, 0.68f, 1.00f}, 0.48f},
    {{-0.10f, -0.30f,  1.00f}, {1.00f, 0.48f, 0.30f}, 0.32f}
}};

float Dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3 Add(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vec3 Sub(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vec3 Mul(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
Vec3 Mul(Vec3 a, Vec3 b) { return {a.x*b.x, a.y*b.y, a.z*b.z}; }
Vec3 Cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
float Length(Vec3 v) { return std::sqrt(std::max(0.0f, Dot(v,v))); }
Vec3 Normalize(Vec3 v) {
    const float length = Length(v);
    return length > 1e-8f ? Mul(v, 1.0f/length) : Vec3{0,1,0};
}
Vec3 Clamp01(Vec3 v) {
    return {std::clamp(v.x,0.0f,1.0f), std::clamp(v.y,0.0f,1.0f), std::clamp(v.z,0.0f,1.0f)};
}
std::uint8_t Byte(float value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value,0.0f,1.0f)*255.0f));
}

void PutPixel(PreviewPixels& image, unsigned x, unsigned y, Vec3 colour, float alpha=1.0f)
{
    if (x>=image.width || y>=image.height) return;
    const std::size_t offset=(static_cast<std::size_t>(y)*image.width+x)*4u;
    colour=Clamp01(colour);
    image.rgba[offset+0]=Byte(colour.x);
    image.rgba[offset+1]=Byte(colour.y);
    image.rgba[offset+2]=Byte(colour.z);
    image.rgba[offset+3]=Byte(alpha);
}

Vec3 Checker(unsigned x,unsigned y)
{
    const bool alternate=((x/12u)+(y/12u))%2u!=0u;
    return alternate ? Vec3{0.115f,0.125f,0.145f} : Vec3{0.075f,0.083f,0.100f};
}

void FillBackground(PreviewPixels& image)
{
    for (unsigned y=0;y<image.height;++y)
        for (unsigned x=0;x<image.width;++x)
            PutPixel(image,x,y,Checker(x,y));
}

Vec3 Shade(Vec3 base,Vec3 normal,Vec3 view,float roughness,float metallic,Vec3 emissive={})
{
    normal=Normalize(normal);
    view=Normalize(view);
    Vec3 result=Add(Mul(base,0.105f),Mul(emissive,0.85f));
    const float m=std::clamp(metallic,0.0f,1.0f);
    const float r=std::clamp(roughness,0.0f,1.0f);
    const float diffuseWeight=1.0f-m*0.72f;
    const float exponent=5.0f+(1.0f-r)*74.0f;
    const Vec3 specBase=Add(Mul(Vec3{0.04f,0.04f,0.04f},1.0f-m),Mul(base,m));
    for (const auto& light:StudioLights)
    {
        const Vec3 toLight=Normalize(Mul(light.direction,-1.0f));
        const float ndotl=std::max(0.0f,Dot(normal,toLight));
        const Vec3 halfVector=Normalize(Add(toLight,view));
        const float spec=std::pow(std::max(0.0f,Dot(normal,halfVector)),exponent)*(0.18f+0.82f*(1.0f-r));
        result=Add(result,Mul(Mul(base,light.colour),ndotl*light.intensity*diffuseWeight));
        result=Add(result,Mul(Mul(specBase,light.colour),spec*light.intensity));
    }
    return Clamp01(result);
}

PreviewPixels RenderTexturePreview(const TextureAsset& texture)
{
    PreviewPixels image;
    FillBackground(image);
    if (!texture.width || !texture.height || texture.rgba.empty())
        throw std::invalid_argument("thumbnail texture has no pixels");
    const float scale=std::min(static_cast<float>(image.width)/texture.width,
                               static_cast<float>(image.height)/texture.height);
    const unsigned drawWidth=std::max(1u,static_cast<unsigned>(std::floor(texture.width*scale)));
    const unsigned drawHeight=std::max(1u,static_cast<unsigned>(std::floor(texture.height*scale)));
    const unsigned startX=(image.width-drawWidth)/2u;
    const unsigned startY=(image.height-drawHeight)/2u;
    for (unsigned y=0;y<drawHeight;++y)
    {
        const unsigned sy=std::min(texture.height-1u,static_cast<unsigned>((static_cast<std::uint64_t>(y)*texture.height)/drawHeight));
        for (unsigned x=0;x<drawWidth;++x)
        {
            const unsigned sx=std::min(texture.width-1u,static_cast<unsigned>((static_cast<std::uint64_t>(x)*texture.width)/drawWidth));
            const std::size_t source=(static_cast<std::size_t>(sy)*texture.width+sx)*4u;
            const float alpha=texture.rgba[source+3]/255.0f;
            const Vec3 colour{texture.rgba[source+0]/255.0f,texture.rgba[source+1]/255.0f,texture.rgba[source+2]/255.0f};
            const Vec3 background=Checker(startX+x,startY+y);
            PutPixel(image,startX+x,startY+y,Add(Mul(colour,alpha),Mul(background,1.0f-alpha)));
        }
    }
    return image;
}

PreviewPixels RenderMaterialPreview(const MaterialAsset& material)
{
    PreviewPixels image;
    FillBackground(image);
    const float cx=(image.width-1u)*0.5f, cy=(image.height-1u)*0.50f;
    const float radius=image.width*0.37f;
    const Vec3 view=Normalize(Sub(StudioCamera.position,StudioCamera.target));
    for (unsigned y=0;y<image.height;++y)
        for (unsigned x=0;x<image.width;++x)
        {
            const float nx=(static_cast<float>(x)-cx)/radius;
            const float ny=(cy-static_cast<float>(y))/radius;
            const float rr=nx*nx+ny*ny;
            if (rr>1.0f) continue;
            const float nz=std::sqrt(std::max(0.0f,1.0f-rr));
            Vec3 colour=Shade(material.colour,{nx,ny,nz},view,material.roughness,material.metallic,material.emissive);
            const float rim=std::pow(std::clamp(1.0f-nz,0.0f,1.0f),2.2f);
            colour=Add(Mul(colour,1.0f-rim*0.12f),Mul(Vec3{0.30f,0.36f,0.48f},rim*0.12f));
            PutPixel(image,x,y,colour);
        }
    return image;
}

struct ProjectedVertex { float x=0,y=0,z=0; Vec3 normal{}; bool valid=false; };

ProjectedVertex Project(Vec3 position,Vec3 normal)
{
    const Vec3 forward=Normalize(Sub(StudioCamera.target,StudioCamera.position));
    const Vec3 right=Normalize(Cross(forward,{0,1,0}));
    const Vec3 up=Normalize(Cross(right,forward));
    const Vec3 relative=Sub(position,StudioCamera.position);
    const float z=Dot(relative,forward);
    if (z<=0.05f) return {};
    const float f=1.0f/std::tan(StudioCamera.fovDegrees*Pi/360.0f);
    return {(Dot(relative,right)*f/z*0.5f+0.5f)*EditorThumbnailCache::PreviewSize,
            (0.5f-Dot(relative,up)*f/z*0.5f)*EditorThumbnailCache::PreviewSize,
            z,normal,true};
}

float Edge(float ax,float ay,float bx,float by,float px,float py)
{
    return (px-ax)*(by-ay)-(py-ay)*(bx-ax);
}

PreviewPixels RenderModelPreview(const ModelAsset& model)
{
    if (model.meshes.empty()) throw std::invalid_argument("thumbnail model has no meshes");
    std::vector<Matrix4> nodeMatrices(model.skeleton.joints.size());
    for (std::size_t i=0;i<model.skeleton.joints.size();++i)
    {
        const auto& node=model.skeleton.joints[i];
        const Matrix4 local=Matrix(node.local);
        nodeMatrices[i]=node.parent>=0 ? Multiply(nodeMatrices[static_cast<std::size_t>(node.parent)],local) : local;
    }

    Vec3 minimum{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    Vec3 maximum{-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
    for (const auto& mesh:model.meshes)
    {
        if (mesh.node>=nodeMatrices.size()) continue;
        for (const auto& vertex:mesh.geometry.vertices)
        {
            const Vec3 p=TransformPoint(nodeMatrices[mesh.node],vertex.position);
            minimum.x=std::min(minimum.x,p.x); minimum.y=std::min(minimum.y,p.y); minimum.z=std::min(minimum.z,p.z);
            maximum.x=std::max(maximum.x,p.x); maximum.y=std::max(maximum.y,p.y); maximum.z=std::max(maximum.z,p.z);
        }
    }
    const Vec3 center=Mul(Add(minimum,maximum),0.5f);
    const Vec3 extent=Sub(maximum,minimum);
    const float radius=std::max({extent.x,extent.y,extent.z})*0.5f;
    if (!(radius>1e-7f) || !std::isfinite(radius)) throw std::invalid_argument("thumbnail model has invalid bounds");

    PreviewPixels image;
    FillBackground(image);
    std::vector<float> depth(static_cast<std::size_t>(image.width)*image.height,std::numeric_limits<float>::infinity());
    const float fit=0.92f/radius;

    for (const auto& mesh:model.meshes)
    {
        if (mesh.node>=nodeMatrices.size() || mesh.geometry.indices.size()<3) continue;
        const Matrix4& transform=nodeMatrices[mesh.node];
        MaterialAsset material;
        if (mesh.material<model.materials.size()) material=model.materials[mesh.material];
        const auto& vertices=mesh.geometry.vertices;
        const auto& indices=mesh.geometry.indices;
        for (std::size_t index=0;index+2<indices.size();index+=3)
        {
            const std::uint32_t ia=indices[index],ib=indices[index+1],ic=indices[index+2];
            if (ia>=vertices.size() || ib>=vertices.size() || ic>=vertices.size()) continue;
            const Vertex* source[3]{&vertices[ia],&vertices[ib],&vertices[ic]};
            ProjectedVertex projected[3];
            for (int i=0;i<3;++i)
            {
                Vec3 p=Mul(Sub(TransformPoint(transform,source[i]->position),center),fit);
                p.y-=0.05f;
                projected[i]=Project(p,TransformNormal(transform,source[i]->normal));
            }
            if (!projected[0].valid || !projected[1].valid || !projected[2].valid) continue;
            const float area=Edge(projected[0].x,projected[0].y,projected[1].x,projected[1].y,projected[2].x,projected[2].y);
            if (std::abs(area)<1e-5f) continue;
            const int minX=std::clamp(static_cast<int>(std::floor(std::min({projected[0].x,projected[1].x,projected[2].x}))),0,static_cast<int>(image.width)-1);
            const int maxX=std::clamp(static_cast<int>(std::ceil(std::max({projected[0].x,projected[1].x,projected[2].x}))),0,static_cast<int>(image.width)-1);
            const int minY=std::clamp(static_cast<int>(std::floor(std::min({projected[0].y,projected[1].y,projected[2].y}))),0,static_cast<int>(image.height)-1);
            const int maxY=std::clamp(static_cast<int>(std::ceil(std::max({projected[0].y,projected[1].y,projected[2].y}))),0,static_cast<int>(image.height)-1);
            for (int y=minY;y<=maxY;++y)
                for (int x=minX;x<=maxX;++x)
                {
                    const float px=x+0.5f,py=y+0.5f;
                    const float w0=Edge(projected[1].x,projected[1].y,projected[2].x,projected[2].y,px,py)/area;
                    const float w1=Edge(projected[2].x,projected[2].y,projected[0].x,projected[0].y,px,py)/area;
                    const float w2=1.0f-w0-w1;
                    if (w0<-0.0001f || w1<-0.0001f || w2<-0.0001f) continue;
                    const float z=w0*projected[0].z+w1*projected[1].z+w2*projected[2].z;
                    const std::size_t pixel=static_cast<std::size_t>(y)*image.width+static_cast<unsigned>(x);
                    if (z>=depth[pixel]) continue;
                    depth[pixel]=z;
                    const Vec3 normal=Normalize(Add(Add(Mul(projected[0].normal,w0),Mul(projected[1].normal,w1)),Mul(projected[2].normal,w2)));
                    PutPixel(image,static_cast<unsigned>(x),static_cast<unsigned>(y),
                             Shade(material.colour,normal,Normalize(StudioCamera.position),material.roughness,material.metallic,material.emissive));
                }
        }
    }
    return image;
}

PreviewPixels GeneratePreview(const std::filesystem::path& path,ThumbnailKind kind)
{
    if (!std::filesystem::is_regular_file(path))
        throw std::runtime_error("thumbnail source is missing: "+path.generic_string());
    std::string extension=path.extension().string();
    std::transform(extension.begin(),extension.end(),extension.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    switch (kind)
    {
    case ThumbnailKind::Material:
        return RenderMaterialPreview(*LoadMaterialAsset(path));
    case ThumbnailKind::Texture:
        if (extension==".plasset")
        {
            auto asset=ReadAsset(path);
            auto texture=std::dynamic_pointer_cast<TextureAsset>(asset);
            if (!texture) throw std::invalid_argument("cooked thumbnail asset is not a texture");
            return RenderTexturePreview(*texture);
        }
        return RenderTexturePreview(ImportTexture(path));
    case ThumbnailKind::Model:
        if (extension==".plasset")
        {
            auto asset=ReadAsset(path);
            auto model=std::dynamic_pointer_cast<ModelAsset>(asset);
            if (!model) throw std::invalid_argument("cooked thumbnail asset is not a model");
            return RenderModelPreview(*model);
        }
        return RenderModelPreview(ImportModel(path));
    }
    throw std::invalid_argument("unsupported thumbnail kind");
}

} // namespace

struct EditorThumbnailCache::Task {
    JobGroup group;
    std::filesystem::path source;
    ThumbnailKind kind=ThumbnailKind::Model;
    std::uint64_t fingerprint=0;
    std::uint64_t serial=0;
    std::vector<std::uint8_t> rgba;
    std::string error;
};

struct EditorThumbnailCache::Entry {
    std::string key;
    std::filesystem::path source;
    ThumbnailKind kind=ThumbnailKind::Model;
    ThumbnailState state=ThumbnailState::Queued;
    TextureHandle texture{};
    std::uint64_t fingerprint=0;
    std::uint64_t serial=0;
    std::uint64_t touched=0;
    std::string error;
    std::unique_ptr<Task> task;
};

EditorThumbnailCache::EditorThumbnailCache() = default;
EditorThumbnailCache::~EditorThumbnailCache() = default;

void EditorThumbnailCache::Initialize() { initialized_=true; }

std::uint64_t EditorThumbnailCache::Fingerprint(const std::filesystem::path& path,std::uint64_t generation)
{
    std::uint64_t result=generation*0x9e3779b97f4a7c15ull;
    try
    {
        if (std::filesystem::is_regular_file(path))
        {
            const auto time=std::filesystem::last_write_time(path).time_since_epoch().count();
            const auto bytes=std::filesystem::file_size(path);
            result^=static_cast<std::uint64_t>(time)+0x9e3779b97f4a7c15ull+(result<<6u)+(result>>2u);
            result^=static_cast<std::uint64_t>(bytes)+0x9e3779b97f4a7c15ull+(result<<6u)+(result>>2u);
        }
    }
    catch (...) {}
    return result?result:1u;
}

void EditorThumbnailCache::RunTask(void* context)
{
    auto& task=*static_cast<Task*>(context);
    try { task.rgba=GeneratePreview(task.source,task.kind).rgba; }
    catch (const std::exception& error) { task.error=error.what(); }
    catch (...) { task.error="unknown thumbnail generation failure"; }
}

void EditorThumbnailCache::Launch(Engine& engine,Entry& entry)
{
    if (entry.state!=ThumbnailState::Queued || entry.task || activeJobs_>=MaxConcurrentThumbnailJobs) return;
    auto task=std::make_unique<Task>();
    task->source=entry.source;
    task->kind=entry.kind;
    task->fingerprint=entry.fingerprint;
    task->serial=entry.serial;
    if (engine.Jobs().TrySubmit(task->group,&EditorThumbnailCache::RunTask,task.get())!=SubmitResult::Accepted) return;
    entry.task=std::move(task);
    entry.state=ThumbnailState::Generating;
    ++activeJobs_;
}

void EditorThumbnailCache::LaunchQueued(Engine& engine)
{
    if (activeJobs_>=MaxConcurrentThumbnailJobs) return;
    std::vector<Entry*> queued;
    queued.reserve(entries_.size());
    for (auto& [key,entry]:entries_)
        if (entry->state==ThumbnailState::Queued && !entry->task) queued.push_back(entry.get());
    std::sort(queued.begin(),queued.end(),[](const Entry* a,const Entry* b){ return a->touched>b->touched; });
    for (Entry* entry:queued)
    {
        if (activeJobs_>=MaxConcurrentThumbnailJobs) break;
        Launch(engine,*entry);
    }
}

void EditorThumbnailCache::RetireReadyTask(Engine& engine,Entry& entry)
{
    if (!entry.task || entry.task->group.pending.load(std::memory_order_acquire)!=0) return;
    if (activeJobs_) --activeJobs_;
    auto task=std::move(entry.task);
    if (task->serial!=entry.serial || task->fingerprint!=entry.fingerprint)
    {
        entry.state=ThumbnailState::Queued;
        entry.error.clear();
        return;
    }
    if (!task->error.empty())
    {
        entry.state=ThumbnailState::Failed;
        entry.error=std::move(task->error);
        return;
    }
    const std::size_t expected=static_cast<std::size_t>(PreviewSize)*PreviewSize*4u;
    if (task->rgba.size()!=expected)
    {
        entry.state=ThumbnailState::Failed;
        entry.error="thumbnail worker returned an invalid RGBA buffer";
        return;
    }
    if (entry.texture) DestroyUiTexture(engine.Graphics(),entry.texture);
    entry.texture=CreateUiTexture(engine.Graphics(),PreviewSize,PreviewSize,
                                  std::span<const std::uint8_t>(task->rgba.data(),task->rgba.size()),false);
    entry.state=ThumbnailState::Ready;
    entry.error.clear();
}

void EditorThumbnailCache::Pump(Engine& engine)
{
    if (!initialized_) return;
    for (auto& [key,entry]:entries_) RetireReadyTask(engine,*entry);
    LaunchQueued(engine);
}

void EditorThumbnailCache::EvictIfNeeded(Engine& engine)
{
    while (entries_.size()>=MaxCachedThumbnails)
    {
        auto victim=entries_.end();
        for (auto it=entries_.begin();it!=entries_.end();++it)
        {
            if (it->second->task) continue;
            if (victim==entries_.end() || it->second->touched<victim->second->touched) victim=it;
        }
        if (victim==entries_.end()) return;
        if (victim->second->texture) DestroyUiTexture(engine.Graphics(),victim->second->texture);
        entries_.erase(victim);
    }
}

ThumbnailView EditorThumbnailCache::Request(Engine& engine,std::string key,std::filesystem::path source,
                                            ThumbnailKind kind,std::uint64_t generation)
{
    if (!initialized_) Initialize();
    const std::uint64_t fingerprint=Fingerprint(source,generation);
    auto found=entries_.find(key);
    if (found==entries_.end())
    {
        EvictIfNeeded(engine);
        auto entry=std::make_unique<Entry>();
        entry->key=key;
        entry->source=source;
        entry->kind=kind;
        entry->fingerprint=fingerprint;
        entry->serial=nextSerial_++;
        entry->touched=touchCounter_++;
        found=entries_.emplace(entry->key,std::move(entry)).first;
    }
    Entry& entry=*found->second;
    entry.touched=touchCounter_++;
    if (entry.fingerprint!=fingerprint || entry.source!=source || entry.kind!=kind)
    {
        entry.fingerprint=fingerprint;
        entry.source=std::move(source);
        entry.kind=kind;
        entry.serial=nextSerial_++;
        entry.error.clear();
        if (entry.texture)
        {
            DestroyUiTexture(engine.Graphics(),entry.texture);
            entry.texture={};
        }
        if (!entry.task) entry.state=ThumbnailState::Queued;
    }
    if (entry.state==ThumbnailState::Queued && !entry.task) Launch(engine,entry);
    return {entry.state,entry.texture,entry.error};
}

void EditorThumbnailCache::Invalidate(Engine& engine,std::string_view key)
{
    const auto found=entries_.find(std::string(key));
    if (found==entries_.end()) return;
    Entry& entry=*found->second;
    ++entry.serial;
    entry.fingerprint^=0x9e3779b97f4a7c15ull;
    entry.error.clear();
    if (entry.texture)
    {
        DestroyUiTexture(engine.Graphics(),entry.texture);
        entry.texture={};
    }
    if (!entry.task) entry.state=ThumbnailState::Queued;
}

void EditorThumbnailCache::InvalidateAll(Engine& engine)
{
    for (auto& [key,entry]:entries_)
    {
        ++entry->serial;
        entry->fingerprint^=0x9e3779b97f4a7c15ull;
        entry->error.clear();
        if (entry->texture)
        {
            DestroyUiTexture(engine.Graphics(),entry->texture);
            entry->texture={};
        }
        if (!entry->task) entry->state=ThumbnailState::Queued;
    }
}

void EditorThumbnailCache::Shutdown(Engine& engine)
{
    if (!initialized_) return;
    for (auto& [key,entry]:entries_)
        if (entry->task) engine.Jobs().Wait(entry->task->group);
    for (auto& [key,entry]:entries_)
    {
        if (entry->texture) DestroyUiTexture(engine.Graphics(),entry->texture);
        entry->texture={};
        entry->task.reset();
    }
    entries_.clear();
    activeJobs_=0;
    initialized_=false;
}

} // namespace PlagC::Editor
