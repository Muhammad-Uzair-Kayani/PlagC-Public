#include "pgcpch.h"
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <charconv>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace PlagC::Editor {
namespace {
constexpr std::size_t MaxBytes = 64 * 1024 * 1024;
constexpr std::size_t MaxEntities = 100000;
using Value = rapidjson::Value;
using Writer = rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>,
                                rapidjson::CrtAllocator, rapidjson::kWriteValidateEncodingFlag>;
[[noreturn]] void Fail(const char *message) { throw std::invalid_argument(message); }
void Fields(const Value &v, std::initializer_list<std::string_view> allowed) {
    if (!v.IsObject()) Fail("Scene field must be an object");
    std::unordered_set<std::string_view> seen;
    for (auto i=v.MemberBegin(); i!=v.MemberEnd(); ++i) {
        std::string_view key(i->name.GetString(),i->name.GetStringLength());
        if (std::find(allowed.begin(),allowed.end(),key)==allowed.end()) Fail("Unknown scene field");
        if (!seen.insert(key).second) Fail("Duplicate scene field");
    }
}
const Value &Get(const Value &v,const char *key) {
    auto i=v.FindMember(key); if(i==v.MemberEnd()) Fail("Missing required scene field"); return i->value;
}
std::string String(const Value &v) {
    if(!v.IsString()) Fail("Scene string expected");
    return {v.GetString(),v.GetStringLength()};
}
float Number(const Value &v) {
    if(!v.IsNumber()) Fail("Scene number expected");
    double d=v.GetDouble();
    if(!std::isfinite(d) || std::abs(d)>std::numeric_limits<float>::max()) Fail("Invalid scene number");
    return static_cast<float>(d);
}
bool Boolean(const Value &v) { if(!v.IsBool()) Fail("Scene boolean expected"); return v.GetBool(); }
Vec3 Vector(const Value &v) {
    if(!v.IsArray() || v.Size()!=3) Fail("Scene vector must contain three numbers");
    return {Number(v[0]),Number(v[1]),Number(v[2])};
}
std::uint64_t ID(const Value &v) {
    auto s=String(v); std::uint64_t id=0;
    auto result=std::from_chars(s.data(),s.data()+s.size(),id);
    if(result.ec!=std::errc{} || result.ptr!=s.data()+s.size() || id==0 || s.front()=='0') Fail("Invalid persistent ID");
    return id;
}
void WriteString(Writer &w,std::string_view text) {
    if(text.size()>MaxBytes) Fail("Scene string exceeds limit");
    if(!w.String(text.data(),static_cast<rapidjson::SizeType>(text.size()))) Fail("Invalid UTF-8 scene string");
}
void WriteNumber(Writer &w,float value) {
    if(!std::isfinite(value)) Fail("Cannot save nonfinite scene number");
    w.Double(value);
}
void WriteVector(Writer &w,Vec3 v) {
    w.StartArray(); WriteNumber(w,v.x); WriteNumber(w,v.y); WriteNumber(w,v.z); w.EndArray();
}
void Scalar(Writer &w,const char *key,float value) { w.Key(key); WriteNumber(w,value); }
void VectorField(Writer &w,const char *key,Vec3 value) { w.Key(key); WriteVector(w,value); }
void AssetPath(const std::string &path) {
    if(path.empty() || path.find('\0')!=std::string::npos || path.find('\\')!=std::string::npos) Fail("Invalid model asset reference");
    auto colon=path.find("://");
    auto relative=colon==std::string::npos ? path : path.substr(colon+3);
    if(relative.empty() || relative.front()=='/' || relative.find(':')!=std::string::npos) Fail("Model asset reference must be relative or mounted");
    if(colon!=std::string::npos && (colon==0 || path.substr(0,colon).find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)) Fail("Invalid asset mount");
    for(const auto &part:std::filesystem::path(relative)) if(part=="..") Fail("Asset path traversal is not allowed");
}
struct Record {
    std::uint64_t id=0,parent=0;
    std::string name;
    Transform transform;
    std::optional<CameraComponent> camera;
    std::optional<LightComponent> light;
    std::string model;
    bool visible=true;
};
void ValidateHierarchy(const std::vector<Record>& records,
                       const std::unordered_set<std::uint64_t>& ids) {
    std::unordered_map<std::uint64_t,std::uint64_t> parents;
    parents.reserve(records.size());
    for(const auto& r:records) {
        if(r.parent) {
            if(r.parent==r.id) Fail("Entity cannot parent itself");
            if(!ids.contains(r.parent)) Fail("Scene parent refers to missing entity");
        }
        parents.emplace(r.id,r.parent);
    }

    // 0=unvisited, 1=visiting, 2=complete. Each node completes once: O(n).
    std::unordered_map<std::uint64_t,std::uint8_t> state;
    state.reserve(records.size());
    std::vector<std::uint64_t> path;
    for(const auto& r:records) {
        if(state[r.id]==2) continue;
        path.clear();
        std::uint64_t current=r.id;
        while(current) {
            auto& mark=state[current];
            if(mark==2) break;
            if(mark==1) Fail("Scene hierarchy contains a cycle");
            mark=1;
            path.push_back(current);
            current=parents.at(current);
        }
        for(auto id:path) state[id]=2;
    }
}
}

std::string SceneDocument::Serialize() const {
    Thread::RequireMain();
    auto records=world_->Snapshot();
    if(records.size()>MaxEntities) Fail("Scene entity limit exceeded");
    rapidjson::StringBuffer buffer; Writer w(buffer);
    w.StartObject(); w.Key("format"); w.String("PlagC.Scene"); w.Key("version"); w.Uint(2);
    w.Key("activeCamera");
    if(auto camera=world_->ActiveCamera()) WriteString(w,std::to_string(PersistentID(camera))); else w.Null();
    w.Key("entities"); w.StartArray();
    for(const auto &r:records) {
        if(r.renderable || r.physics || r.animator) Fail("Scene v2 cannot save renderable, physics or animator components yet");
        w.StartObject(); w.Key("id"); WriteString(w,std::to_string(PersistentID(r.entity)));
        w.Key("parent");
        if(auto parent=world_->Parent(r.entity)) WriteString(w,std::to_string(PersistentID(parent))); else w.Null();
        w.Key("name"); WriteString(w,r.name); w.Key("transform"); w.StartObject();
        VectorField(w,"position",r.transform.position); VectorField(w,"rotation",r.transform.rotationDegrees); VectorField(w,"scale",r.transform.scale); w.EndObject();
        if(r.camera) {
            const auto &c=*r.camera; w.Key("camera"); w.StartObject(); VectorField(w,"target",c.target);
            Scalar(w,"fov",c.fov); Scalar(w,"near",c.nearClip); Scalar(w,"far",c.farClip); Scalar(w,"aspect",c.aspect); w.EndObject();
        }
        if(r.light) {
            const auto &l=r.light->light; w.Key("light"); w.StartObject();
            w.Key("type"); w.Int(static_cast<int>(l.type)); VectorField(w,"position",l.position);
            VectorField(w,"direction",l.direction); VectorField(w,"colour",l.colour);
            Scalar(w,"power",l.power); Scalar(w,"radius",l.radius); Scalar(w,"inner",l.innerAngle); Scalar(w,"outer",l.outerAngle); w.EndObject();
        }
        if(r.model) {
            if(!r.model->asset) Fail("Cannot save an empty model handle");
            const auto &path=r.model->asset->metadata.path; AssetPath(path);
            w.Key("model"); w.StartObject(); w.Key("asset"); WriteString(w,path);
            w.Key("visible"); w.Bool(r.model->visible); w.EndObject();
        }
        w.EndObject();
        if(buffer.GetSize()>MaxBytes) Fail("Scene exceeds size limit");
    }
    w.EndArray(); w.EndObject();
    if(buffer.GetSize()>MaxBytes) Fail("Scene exceeds size limit");
    return {buffer.GetString(),buffer.GetSize()};
}

std::unique_ptr<SceneDocument> SceneDocument::Deserialize(std::string_view text, ModelResolver resolve) {
    Thread::RequireMain();
    if(text.size()>MaxBytes) Fail("Scene exceeds size limit");
    if(text.find('\0')!=std::string_view::npos) Fail("Unexpected NUL byte in scene JSON");
    rapidjson::Document json;
    json.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(text.data(),text.size());
    if(json.HasParseError()) Fail("Invalid scene JSON");
    Fields(json,{"format","version","activeCamera","entities"});
    if(String(Get(json,"format"))!="PlagC.Scene") Fail("Unknown scene format");
    auto &versionValue=Get(json,"version");
    if(!versionValue.IsUint()) Fail("Unsupported scene version");
    const unsigned version=versionValue.GetUint();
    if(version!=1 && version!=2) Fail("Unsupported scene version");
    auto &entities=Get(json,"entities");
    if(!entities.IsArray() || entities.Size()>MaxEntities) Fail("Invalid scene entity array");
    auto &active=Get(json,"activeCamera"); std::uint64_t activeID=active.IsNull()?0:ID(active);
    std::vector<Record> records; records.reserve(entities.Size());
    std::unordered_set<std::uint64_t> ids;
    ids.reserve(entities.Size());
    for(const auto &v:entities.GetArray()) {
        if(version==1) Fields(v,{"id","name","transform","camera","light","model"});
        else Fields(v,{"id","parent","name","transform","camera","light","model"});
        Record r; r.id=ID(Get(v,"id")); if(!ids.insert(r.id).second) Fail("Duplicate persistent entity ID");
        if(version==2) {
            const auto& parent=Get(v,"parent");
            r.parent=parent.IsNull()?0:ID(parent);
        }
        r.name=String(Get(v,"name")); auto &t=Get(v,"transform"); Fields(t,{"position","rotation","scale"});
        r.transform={Vector(Get(t,"position")),Vector(Get(t,"rotation")),Vector(Get(t,"scale"))};
        if(v.HasMember("camera")) {
            auto &c=v["camera"]; Fields(c,{"target","fov","near","far","aspect"});
            r.camera=CameraComponent{Vector(Get(c,"target")),Number(Get(c,"fov")),Number(Get(c,"near")),Number(Get(c,"far")),Number(Get(c,"aspect"))};
        }
        if(v.HasMember("light")) {
            auto &l=v["light"]; Fields(l,{"type","position","direction","colour","power","radius","inner","outer"});
            auto &type=Get(l,"type"); if(!type.IsInt() || type.GetInt()<0 || type.GetInt()>2) Fail("Invalid light type");
            LightDesc d; d.type=static_cast<LightType>(type.GetInt()); d.position=Vector(Get(l,"position"));
            d.direction=Vector(Get(l,"direction")); d.colour=Vector(Get(l,"colour"));
            d.power=Number(Get(l,"power")); d.radius=Number(Get(l,"radius")); d.innerAngle=Number(Get(l,"inner")); d.outerAngle=Number(Get(l,"outer")); r.light=LightComponent{d};
        }
        if(v.HasMember("model")) {
            auto &m=v["model"]; Fields(m,{"asset","visible"}); r.model=String(Get(m,"asset")); AssetPath(r.model); r.visible=Boolean(Get(m,"visible"));
        }
        records.push_back(std::move(r));
    }

    ValidateHierarchy(records,ids);
    if(activeID && !ids.contains(activeID)) Fail("Active camera refers to missing entity");

    // Validate every component and relationship before invoking asset loaders.
    auto world=std::make_unique<World>(); std::unordered_map<std::uint64_t,Entity> entitiesByID;
    entitiesByID.reserve(records.size());
    for(const auto &r:records) {
        auto e=world->Create(r.name); entitiesByID.emplace(r.id,e); world->SetTransform(e,r.transform);
        if(r.camera) world->SetCamera(e,*r.camera);
        if(r.light) world->SetLight(e,*r.light);
    }
    for(const auto &r:records)
        if(r.parent) world->SetParent(entitiesByID.at(r.id),entitiesByID.at(r.parent));
    if(activeID) world->SetActiveCamera(entitiesByID.at(activeID));

    for(const auto &r:records) if(!r.model.empty()) {
        if(!resolve) Fail("Model resolver required to load this scene");
        auto asset=resolve(r.model); if(!asset) Fail("Scene model could not be resolved");
        if(asset->metadata.path!=r.model) Fail("Resolved model path does not match scene reference");
        world->SetModel(entitiesByID.at(r.id),{asset,r.visible});
    }
    auto document=std::make_unique<SceneDocument>(std::move(world));
    document->persistentIDs_.clear();
    for(const auto &[id,e]:entitiesByID) document->persistentIDs_.emplace(e.value,id);
    return document;
}

void SceneDocument::Save(const std::filesystem::path &path) {
    Thread::RequireMain();
    const auto data=Serialize();
    auto parent=path.parent_path(); if(parent.empty()) parent=".";
    static std::atomic<std::uint64_t> serial{0}; std::filesystem::path staging;
    for(unsigned attempt=0;attempt<100;++attempt) {
        auto candidate=parent/(".plagc-scene-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(serial++));
        std::error_code ec;
        if(std::filesystem::create_directory(candidate,ec)) { staging=candidate; break; }
        if(ec) throw std::filesystem::filesystem_error("Cannot stage scene",candidate,ec);
    }
    if(staging.empty()) throw std::runtime_error("Cannot reserve scene staging directory");
    struct Cleanup { std::filesystem::path p; ~Cleanup(){std::error_code ec; std::filesystem::remove_all(p,ec);} } cleanup{staging};
    auto temporary=staging/"scene.json";
    std::ofstream file(temporary,std::ios::binary); file.exceptions(std::ios::badbit|std::ios::failbit);
    file.write(data.data(),static_cast<std::streamsize>(data.size())); file.flush(); file.close();
#if defined(_WIN32)
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Replace scene file");
#else
    std::filesystem::rename(temporary,path);
#endif
    history_.MarkSaved();
}

std::unique_ptr<SceneDocument> SceneDocument::Load(const std::filesystem::path &path,ModelResolver resolve) {
    Thread::RequireMain();
    std::ifstream file(path,std::ios::binary); if(!file) throw std::runtime_error("Cannot open scene file");
    std::string data; char buffer[8192];
    while(file) {
        file.read(buffer,sizeof(buffer)); data.append(buffer,static_cast<std::size_t>(file.gcount()));
        if(data.size()>MaxBytes) Fail("Scene exceeds size limit");
    }
    if(!file.eof() || file.bad()) throw std::runtime_error("Cannot read scene file");
    return Deserialize(data,std::move(resolve));
}
}
