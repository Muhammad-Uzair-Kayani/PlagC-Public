#include "pgcpch.h"
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>

#if defined(_MSC_VER)
#ifndef _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#endif
#pragma warning(disable : 5054)
#endif
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
constexpr std::size_t MaxGeometryElements = 1000000;
constexpr std::size_t MaxAnimationItems = 4096;
constexpr unsigned CurrentSceneVersion = 4;
constexpr unsigned SceneParseFlags =
    static_cast<unsigned>(rapidjson::kParseValidateEncodingFlag) |
    static_cast<unsigned>(rapidjson::kParseIterativeFlag);
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
    const double d=v.GetDouble();
    if(!std::isfinite(d) || std::abs(d)>std::numeric_limits<float>::max()) Fail("Invalid scene number");
    return static_cast<float>(d);
}

double DoubleNumber(const Value &v) {
    if(!v.IsNumber()) Fail("Scene number expected");
    const double d=v.GetDouble();
    if(!std::isfinite(d)) Fail("Invalid scene number");
    return d;
}

unsigned Unsigned(const Value &v) {
    if(!v.IsUint()) Fail("Scene unsigned integer expected");
    return v.GetUint();
}

bool Boolean(const Value &v) { if(!v.IsBool()) Fail("Scene boolean expected"); return v.GetBool(); }

Vec3 Vector(const Value &v) {
    if(!v.IsArray() || v.Size()!=3) Fail("Scene vector must contain three numbers");
    return {Number(v[0]),Number(v[1]),Number(v[2])};
}

Transform TransformValue(const Value& v) {
    Fields(v,{"position","rotation","scale"});
    return {Vector(Get(v,"position")),Vector(Get(v,"rotation")),Vector(Get(v,"scale"))};
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

void WriteDouble(Writer &w,double value) {
    if(!std::isfinite(value)) Fail("Cannot save nonfinite scene number");
    w.Double(value);
}

void WriteVector(Writer &w,Vec3 v) {
    w.StartArray(); WriteNumber(w,v.x); WriteNumber(w,v.y); WriteNumber(w,v.z); w.EndArray();
}

void Scalar(Writer &w,const char *key,float value) { w.Key(key); WriteNumber(w,value); }
void VectorField(Writer &w,const char *key,Vec3 value) { w.Key(key); WriteVector(w,value); }

void WriteTransform(Writer& w,const Transform& transform) {
    w.StartObject();
    VectorField(w,"position",transform.position);
    VectorField(w,"rotation",transform.rotationDegrees);
    VectorField(w,"scale",transform.scale);
    w.EndObject();
}

void AssetPath(const std::string &path) {
    if(path.empty() || path.find('\0')!=std::string::npos || path.find('\\')!=std::string::npos) Fail("Invalid asset reference");
    const auto colon=path.find("://");
    const auto relative=colon==std::string::npos ? path : path.substr(colon+3);
    if(relative.empty() || relative.front()=='/' || relative.find(':')!=std::string::npos) Fail("Asset reference must be relative or mounted");
    if(colon!=std::string::npos && (colon==0 || path.substr(0,colon).find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)) Fail("Invalid asset mount");
    for(const auto &part:std::filesystem::path(relative)) if(part=="..") Fail("Asset path traversal is not allowed");
}

void WriteAnimationState(Writer& w,const AnimationState& state) {
    w.StartObject();
    w.Key("clip"); w.Uint(state.clip);
    w.Key("time"); WriteDouble(w,state.time);
    Scalar(w,"speed",state.speed);
    w.Key("playing"); w.Bool(state.playing);
    w.Key("loop"); w.Bool(state.loop);
    w.EndObject();
}

AnimationState ReadAnimationState(const Value& value) {
    Fields(value,{"clip","time","speed","playing","loop"});
    AnimationState state;
    state.clip=Unsigned(Get(value,"clip"));
    state.time=DoubleNumber(Get(value,"time"));
    if(state.time<0.0) Fail("Animation time cannot be negative");
    state.speed=Number(Get(value,"speed"));
    state.playing=Boolean(Get(value,"playing"));
    state.loop=Boolean(Get(value,"loop"));
    return state;
}

void WriteAnimationGraph(Writer& w,const AnimationStateMachine& graph) {
    w.StartObject();
    w.Key("entry"); w.Uint(graph.entry);
    w.Key("parameters"); w.StartArray();
    for(const auto& parameter:graph.parameters) {
        w.StartObject(); w.Key("name"); WriteString(w,parameter.name);
        w.Key("type"); w.Int(static_cast<int>(parameter.type));
        Scalar(w,"initial",parameter.initial); w.EndObject();
    }
    w.EndArray();
    w.Key("states"); w.StartArray();
    for(const auto& state:graph.states) {
        w.StartObject(); w.Key("name"); WriteString(w,state.name);
        w.Key("clip"); w.Uint(state.clip); w.Key("loop"); w.Bool(state.loop);
        Scalar(w,"speed",state.speed); w.EndObject();
    }
    w.EndArray();
    w.Key("transitions"); w.StartArray();
    for(const auto& transition:graph.transitions) {
        w.StartObject(); w.Key("from"); w.Uint(transition.from); w.Key("to"); w.Uint(transition.to);
        Scalar(w,"crossfade",transition.crossfade);
        w.Key("conditions"); w.StartArray();
        for(const auto& condition:transition.conditions) {
            w.StartObject(); w.Key("parameter"); WriteString(w,condition.parameter);
            w.Key("compare"); w.Int(static_cast<int>(condition.compare));
            Scalar(w,"value",condition.value); w.EndObject();
        }
        w.EndArray(); w.EndObject();
    }
    w.EndArray(); w.EndObject();
}

AnimationStateMachine ReadAnimationGraph(const Value& value) {
    Fields(value,{"entry","parameters","states","transitions"});
    AnimationStateMachine graph;
    graph.entry=Unsigned(Get(value,"entry"));
    const auto& parameters=Get(value,"parameters");
    const auto& states=Get(value,"states");
    const auto& transitions=Get(value,"transitions");
    if(!parameters.IsArray() || parameters.Size()>MaxAnimationItems ||
       !states.IsArray() || states.Size()>MaxAnimationItems ||
       !transitions.IsArray() || transitions.Size()>MaxAnimationItems)
        Fail("Invalid animation controller array");
    for(const auto& item:parameters.GetArray()) {
        Fields(item,{"name","type","initial"});
        const auto& type=Get(item,"type");
        if(!type.IsInt() || type.GetInt()<0 || type.GetInt()>static_cast<int>(AnimationParameterType::Trigger))
            Fail("Invalid animation parameter type");
        graph.parameters.push_back({String(Get(item,"name")),static_cast<AnimationParameterType>(type.GetInt()),Number(Get(item,"initial"))});
    }
    for(const auto& item:states.GetArray()) {
        Fields(item,{"name","clip","loop","speed"});
        graph.states.push_back({String(Get(item,"name")),Unsigned(Get(item,"clip")),Boolean(Get(item,"loop")),Number(Get(item,"speed"))});
    }
    for(const auto& item:transitions.GetArray()) {
        Fields(item,{"from","to","crossfade","conditions"});
        AnimationTransition transition;
        transition.from=Unsigned(Get(item,"from")); transition.to=Unsigned(Get(item,"to"));
        transition.crossfade=Number(Get(item,"crossfade"));
        const auto& conditions=Get(item,"conditions");
        if(!conditions.IsArray() || conditions.Size()>MaxAnimationItems) Fail("Invalid animation condition array");
        for(const auto& conditionValue:conditions.GetArray()) {
            Fields(conditionValue,{"parameter","compare","value"});
            const auto& compare=Get(conditionValue,"compare");
            if(!compare.IsInt() || compare.GetInt()<0 || compare.GetInt()>static_cast<int>(AnimationCompare::NotEqual))
                Fail("Invalid animation condition comparison");
            transition.conditions.push_back({String(Get(conditionValue,"parameter")),static_cast<AnimationCompare>(compare.GetInt()),Number(Get(conditionValue,"value"))});
        }
        graph.transitions.push_back(std::move(transition));
    }
    return graph;
}

struct ControllerRecord {
    AnimationStateMachine graph;
    unsigned current=0;
    std::vector<float> values;
};

struct AnimatorRecord {
    std::string source;
    AnimationState state;
    std::optional<ControllerRecord> controller;
    std::optional<AnimationUpperBody> upperBody;
};

void WriteAnimator(Writer& w,const AnimatorComponent& animator) {
    w.StartObject();
    w.Key("source");
    if(animator.animationSource) {
        AssetPath(animator.animationSource->metadata.path);
        WriteString(w,animator.animationSource->metadata.path);
    } else w.Null();
    w.Key("state"); WriteAnimationState(w,animator.state);
    w.Key("controller");
    if(animator.controller) {
        const auto& controller=*animator.controller;
        w.StartObject(); w.Key("graph"); WriteAnimationGraph(w,controller.Graph());
        w.Key("current"); w.Uint(controller.CurrentIndex());
        w.Key("values"); w.StartArray();
        for(const auto& parameter:controller.Graph().parameters) WriteNumber(w,controller.Value(parameter.name));
        w.EndArray(); w.EndObject();
    } else w.Null();
    w.Key("upperBody");
    if(animator.upperBody) {
        w.StartObject(); w.Key("pose"); WriteAnimationState(w,animator.upperBody->pose);
        w.Key("root"); WriteString(w,animator.upperBody->root);
        Scalar(w,"weight",animator.upperBody->weight);
        Scalar(w,"recoil",animator.upperBody->recoilRadians); w.EndObject();
    } else w.Null();
    w.EndObject();
}

AnimatorRecord ReadAnimator(const Value& value) {
    Fields(value,{"source","state","controller","upperBody"});
    AnimatorRecord record;
    const auto& source=Get(value,"source");
    if(!source.IsNull()) { record.source=String(source); AssetPath(record.source); }
    record.state=ReadAnimationState(Get(value,"state"));
    const auto& controller=Get(value,"controller");
    if(!controller.IsNull()) {
        Fields(controller,{"graph","current","values"});
        ControllerRecord parsed;
        parsed.graph=ReadAnimationGraph(Get(controller,"graph"));
        parsed.current=Unsigned(Get(controller,"current"));
        const auto& values=Get(controller,"values");
        if(!values.IsArray() || values.Size()!=parsed.graph.parameters.size()) Fail("Invalid animation controller values");
        parsed.values.reserve(values.Size());
        for(const auto& item:values.GetArray()) parsed.values.push_back(Number(item));
        record.controller=std::move(parsed);
    }
    const auto& upper=Get(value,"upperBody");
    if(!upper.IsNull()) {
        Fields(upper,{"pose","root","weight","recoil"});
        AnimationUpperBody body;
        body.pose=ReadAnimationState(Get(upper,"pose")); body.root=String(Get(upper,"root"));
        body.weight=Number(Get(upper,"weight")); body.recoilRadians=Number(Get(upper,"recoil"));
        record.upperBody=std::move(body);
    }
    return record;
}

void WriteGeometry(Writer& w,const ColliderGeometry& geometry) {
    w.StartObject();
    w.Key("vertices"); w.StartArray(); for(const auto& vertex:geometry.vertices) WriteVector(w,vertex); w.EndArray();
    w.Key("indices"); w.StartArray(); for(const auto index:geometry.indices) w.Uint(index); w.EndArray();
    w.Key("children"); w.StartArray();
    for(const auto& child:geometry.children) {
        w.StartObject(); w.Key("type"); w.Int(static_cast<int>(child.type));
        VectorField(w,"dimensions",child.dimensions); w.Key("transform"); WriteTransform(w,child.transform); w.EndObject();
    }
    w.EndArray();
    w.Key("heights"); w.StartArray(); for(const float height:geometry.heights) WriteNumber(w,height); w.EndArray();
    w.Key("samples"); w.Uint(geometry.samples); VectorField(w,"offset",geometry.offset); VectorField(w,"spacing",geometry.spacing);
    w.EndObject();
}

std::shared_ptr<const ColliderGeometry> ReadGeometry(const Value& value) {
    Fields(value,{"vertices","indices","children","heights","samples","offset","spacing"});
    auto geometry=std::make_shared<ColliderGeometry>();
    const auto& vertices=Get(value,"vertices"); const auto& indices=Get(value,"indices");
    const auto& children=Get(value,"children"); const auto& heights=Get(value,"heights");
    if(!vertices.IsArray() || vertices.Size()>MaxGeometryElements ||
       !indices.IsArray() || indices.Size()>MaxGeometryElements*3 ||
       !children.IsArray() || children.Size()>MaxAnimationItems ||
       !heights.IsArray() || heights.Size()>MaxGeometryElements)
        Fail("Invalid collider geometry array");
    geometry->vertices.reserve(vertices.Size()); for(const auto& item:vertices.GetArray()) geometry->vertices.push_back(Vector(item));
    geometry->indices.reserve(indices.Size()); for(const auto& item:indices.GetArray()) geometry->indices.push_back(Unsigned(item));
    geometry->children.reserve(children.Size());
    for(const auto& item:children.GetArray()) {
        Fields(item,{"type","dimensions","transform"}); const auto& type=Get(item,"type");
        if(!type.IsInt() || type.GetInt()<0 || type.GetInt()>static_cast<int>(ColliderType::Heightfield)) Fail("Invalid child collider type");
        ColliderChild child; child.type=static_cast<ColliderType>(type.GetInt()); child.dimensions=Vector(Get(item,"dimensions"));
        child.transform=TransformValue(Get(item,"transform")); geometry->children.push_back(std::move(child));
    }
    geometry->heights.reserve(heights.Size()); for(const auto& item:heights.GetArray()) geometry->heights.push_back(Number(item));
    geometry->samples=Unsigned(Get(value,"samples")); geometry->offset=Vector(Get(value,"offset")); geometry->spacing=Vector(Get(value,"spacing"));
    return geometry;
}

void WritePhysics(Writer& w,const PhysicsComponent& physics) {
    const auto& body=physics.body; const auto& collider=physics.collider;
    w.StartObject();
    w.Key("motion"); w.Int(static_cast<int>(body.motion)); w.Key("layer"); w.Int(static_cast<int>(body.layer));
    w.Key("mask"); w.Uint(body.collisionMask); Scalar(w,"mass",body.mass); Scalar(w,"gravityScale",body.gravityScale);
    Scalar(w,"linearDamping",body.linearDamping); Scalar(w,"angularDamping",body.angularDamping);
    w.Key("allowSleep"); w.Bool(body.allowSleep); w.Key("continuous"); w.Bool(body.continuous); w.Key("sensor"); w.Bool(body.sensor);
    w.Key("group"); w.Uint(body.collisionGroup);
    w.Key("collider"); w.StartObject(); w.Key("type"); w.Int(static_cast<int>(collider.type));
    VectorField(w,"dimensions",collider.dimensions); Scalar(w,"friction",collider.material.friction); Scalar(w,"restitution",collider.material.restitution);
    VectorField(w,"center",collider.centerOfMassOffset); w.Key("geometry");
    if(collider.geometry) WriteGeometry(w,*collider.geometry); else w.Null();
    w.EndObject(); w.EndObject();
}

PhysicsComponent ReadPhysics(const Value& value) {
    Fields(value,{"motion","layer","mask","mass","gravityScale","linearDamping","angularDamping","allowSleep","continuous","sensor","group","collider"});
    PhysicsComponent physics;
    const auto& motion=Get(value,"motion"); const auto& layer=Get(value,"layer");
    if(!motion.IsInt() || motion.GetInt()<0 || motion.GetInt()>static_cast<int>(MotionType::Kinematic)) Fail("Invalid physics motion type");
    if(!layer.IsInt() || layer.GetInt()<0 || layer.GetInt()>=static_cast<int>(CollisionLayer::Count)) Fail("Invalid physics collision layer");
    physics.body.motion=static_cast<MotionType>(motion.GetInt()); physics.body.layer=static_cast<CollisionLayer>(layer.GetInt());
    physics.body.collisionMask=Unsigned(Get(value,"mask"));
    if((physics.body.collisionMask & ~AllCollisionLayers)!=0) Fail("Physics collision mask contains unknown layers");
    physics.body.mass=Number(Get(value,"mass")); physics.body.gravityScale=Number(Get(value,"gravityScale"));
    physics.body.linearDamping=Number(Get(value,"linearDamping")); physics.body.angularDamping=Number(Get(value,"angularDamping"));
    physics.body.allowSleep=Boolean(Get(value,"allowSleep")); physics.body.continuous=Boolean(Get(value,"continuous")); physics.body.sensor=Boolean(Get(value,"sensor"));
    physics.body.collisionGroup=Unsigned(Get(value,"group"));
    const auto& collider=Get(value,"collider"); Fields(collider,{"type","dimensions","friction","restitution","center","geometry"});
    const auto& type=Get(collider,"type");
    if(!type.IsInt() || type.GetInt()<0 || type.GetInt()>static_cast<int>(ColliderType::Heightfield)) Fail("Invalid collider type");
    physics.collider.type=static_cast<ColliderType>(type.GetInt()); physics.collider.dimensions=Vector(Get(collider,"dimensions"));
    physics.collider.material.friction=Number(Get(collider,"friction")); physics.collider.material.restitution=Number(Get(collider,"restitution"));
    physics.collider.centerOfMassOffset=Vector(Get(collider,"center"));
    const auto& geometry=Get(collider,"geometry"); if(!geometry.IsNull()) physics.collider.geometry=ReadGeometry(geometry);
    return physics;
}

struct Record {
    std::uint64_t id=0,parent=0;
    std::string name;
    Transform transform;
    std::optional<CameraComponent> camera;
    std::optional<LightComponent> light;
    std::string model;
    bool visible=true;
    bool castShadows=true;
    std::vector<std::string> materialOverrides;
    std::optional<PhysicsComponent> physics;
    std::optional<AnimatorRecord> animator;
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
    std::unordered_map<std::uint64_t,std::uint8_t> state;
    state.reserve(records.size());
    std::vector<std::uint64_t> path;
    for(const auto& r:records) {
        if(state[r.id]==2) continue;
        path.clear(); std::uint64_t current=r.id;
        while(current) {
            auto& mark=state[current];
            if(mark==2) break;
            if(mark==1) Fail("Scene hierarchy contains a cycle");
            mark=1; path.push_back(current); current=parents.at(current);
        }
        for(auto id:path) state[id]=2;
    }
}

} // namespace

std::string SceneDocument::Serialize() const {
    Thread::RequireMain();
    auto records=world_->Snapshot();
    if(records.size()>MaxEntities) Fail("Scene entity limit exceeded");
    rapidjson::StringBuffer buffer; Writer w(buffer);
    w.StartObject(); w.Key("format"); w.String("PlagC.Scene"); w.Key("version"); w.Uint(CurrentSceneVersion);
    w.Key("activeCamera");
    if(auto camera=world_->ActiveCamera()) WriteString(w,std::to_string(PersistentID(camera))); else w.Null();
    w.Key("entities"); w.StartArray();
    for(const auto &r:records) {
        if(r.renderable) Fail("Scene v4 cannot save raw RenderableComponent renderer handles; use a ModelComponent asset reference");
        w.StartObject(); w.Key("id"); WriteString(w,std::to_string(PersistentID(r.entity)));
        w.Key("parent");
        if(auto parent=world_->Parent(r.entity)) WriteString(w,std::to_string(PersistentID(parent))); else w.Null();
        w.Key("name"); WriteString(w,r.name); w.Key("transform"); WriteTransform(w,r.transform);
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
            w.Key("visible"); w.Bool(r.model->visible); w.Key("castShadows"); w.Bool(r.model->castShadows);
            w.Key("materials"); w.StartArray();
            for(const auto& material:r.model->materialOverrides) {
                if(material) { AssetPath(material->assetPath); WriteString(w,material->assetPath); }
                else w.Null();
            }
            w.EndArray(); w.EndObject();
        }
        if(r.physics) { w.Key("physics"); WritePhysics(w,*r.physics); }
        if(r.animator) { w.Key("animator"); WriteAnimator(w,*r.animator); }
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
    rapidjson::Document json; json.Parse<SceneParseFlags>(text.data(),text.size());
    if(json.HasParseError()) Fail("Invalid scene JSON");
    Fields(json,{"format","version","activeCamera","entities"});
    if(String(Get(json,"format"))!="PlagC.Scene") Fail("Unknown scene format");
    auto &versionValue=Get(json,"version"); if(!versionValue.IsUint()) Fail("Unsupported scene version");
    const unsigned version=versionValue.GetUint();
    if(version<1 || version>CurrentSceneVersion) Fail("Unsupported scene version");
    auto &entities=Get(json,"entities");
    if(!entities.IsArray() || entities.Size()>MaxEntities) Fail("Invalid scene entity array");
    auto &active=Get(json,"activeCamera"); const std::uint64_t activeID=active.IsNull()?0:ID(active);
    std::vector<Record> records; records.reserve(entities.Size());
    std::unordered_set<std::uint64_t> ids; ids.reserve(entities.Size());
    std::uint64_t maxID=0;
    for(const auto &v:entities.GetArray()) {
        if(version==1) Fields(v,{"id","name","transform","camera","light","model"});
        else if(version<4) Fields(v,{"id","parent","name","transform","camera","light","model"});
        else Fields(v,{"id","parent","name","transform","camera","light","model","physics","animator"});
        Record r; r.id=ID(Get(v,"id")); maxID=std::max(maxID,r.id); if(!ids.insert(r.id).second) Fail("Duplicate persistent entity ID");
        if(version>=2) { const auto& parent=Get(v,"parent"); r.parent=parent.IsNull()?0:ID(parent); }
        r.name=String(Get(v,"name")); r.transform=TransformValue(Get(v,"transform"));
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
            auto &m=v["model"];
            if(version<3) {
                Fields(m,{"asset","visible"}); r.model=String(Get(m,"asset")); AssetPath(r.model); r.visible=Boolean(Get(m,"visible"));
            } else {
                Fields(m,{"asset","visible","castShadows","materials"});
                r.model=String(Get(m,"asset")); AssetPath(r.model); r.visible=Boolean(Get(m,"visible")); r.castShadows=Boolean(Get(m,"castShadows"));
                const auto& materials=Get(m,"materials");
                if(!materials.IsArray() || materials.Size()>4096) Fail("Invalid material override array");
                r.materialOverrides.reserve(materials.Size());
                for(const auto& value:materials.GetArray()) {
                    if(value.IsNull()) { r.materialOverrides.emplace_back(); continue; }
                    auto material=String(value); AssetPath(material); r.materialOverrides.push_back(std::move(material));
                }
            }
        }
        if(version>=4 && v.HasMember("physics")) r.physics=ReadPhysics(v["physics"]);
        if(version>=4 && v.HasMember("animator")) r.animator=ReadAnimator(v["animator"]);
        records.push_back(std::move(r));
    }

    ValidateHierarchy(records,ids);
    if(activeID && !ids.contains(activeID)) Fail("Active camera refers to missing entity");
    auto world=std::make_unique<World>(); std::unordered_map<std::uint64_t,Entity> entitiesByID;
    std::unordered_map<std::uint64_t,AssetHandle<ModelAsset>> modelsByID;
    entitiesByID.reserve(records.size()); modelsByID.reserve(records.size());
    for(const auto &r:records) {
        auto e=world->Create(r.name); entitiesByID.emplace(r.id,e); world->SetTransform(e,r.transform);
        if(r.camera) world->SetCamera(e,*r.camera);
        if(r.light) world->SetLight(e,*r.light);
    }
    for(const auto &r:records) if(r.parent) world->SetParent(entitiesByID.at(r.id),entitiesByID.at(r.parent));
    if(activeID) {
        const auto camera=entitiesByID.at(activeID);
        if(!world->Read(camera).camera) Fail("Active camera entity has no camera component");
        world->SetActiveCamera(camera);
    }

    for(const auto &r:records) if(!r.model.empty()) {
        if(!resolve) Fail("Model resolver required to load this scene");
        auto asset=resolve(r.model); if(!asset) Fail("Scene model could not be resolved");
        if(asset->metadata.path!=r.model) Fail("Resolved model path does not match scene reference");
        if(r.materialOverrides.size()>asset->materials.size()) Fail("Scene material override count exceeds model material slots");
        ModelComponent model{asset,r.visible,r.castShadows};
        model.materialOverrides.resize(r.materialOverrides.size());
        for(std::size_t i=0;i<r.materialOverrides.size();++i) {
            if(r.materialOverrides[i].empty()) continue;
            auto material=LoadMaterialAsset(r.materialOverrides[i]);
            if(!material || material->assetPath!=r.materialOverrides[i]) Fail("Scene material could not be resolved");
            model.materialOverrides[i]=std::move(material);
        }
        world->SetModel(entitiesByID.at(r.id),model); modelsByID.emplace(r.id,std::move(asset));
    }

    for(const auto& r:records) if(r.physics) {
        ValidatePhysicsBody({*r.physics,r.transform});
        world->SetPhysics(entitiesByID.at(r.id),*r.physics);
    }

    for(const auto& r:records) if(r.animator) {
        AnimatorComponent animator; animator.state=r.animator->state; animator.upperBody=r.animator->upperBody;
        AssetHandle<ModelAsset> animationAsset;
        if(!r.animator->source.empty()) {
            if(!resolve) Fail("Model resolver required to load animator source");
            animationAsset=resolve(r.animator->source); if(!animationAsset) Fail("Animator source could not be resolved");
            if(animationAsset->metadata.path!=r.animator->source) Fail("Resolved animator source path does not match scene reference");
            animator.animationSource=animationAsset;
        }
        if(!animationAsset) {
            const auto model=modelsByID.find(r.id); if(model!=modelsByID.end()) animationAsset=model->second;
        }
        const unsigned clips=animationAsset?static_cast<unsigned>(animationAsset->animations.size()):0u;
        if(clips && animator.state.clip>=clips) Fail("Animator clip index is out of range");
        if(r.animator->upperBody && clips && r.animator->upperBody->pose.clip>=clips) Fail("Upper-body animation clip index is out of range");
        if(r.animator->controller) {
            if(!clips) Fail("Animation controller requires a resolved animation source");
            auto controller=AnimationController(r.animator->controller->graph,clips);
            if(r.animator->controller->current>=controller.Graph().states.size()) Fail("Animation controller current state is out of range");
            controller.SetState(r.animator->controller->current);
            for(std::size_t i=0;i<controller.Graph().parameters.size();++i) {
                const auto& parameter=controller.Graph().parameters[i]; const float value=r.animator->controller->values[i];
                switch(parameter.type) {
                case AnimationParameterType::Float: controller.SetFloat(parameter.name,value); break;
                case AnimationParameterType::Bool: controller.SetBool(parameter.name,value!=0.0f); break;
                case AnimationParameterType::Trigger:
                    if(value!=0.0f) controller.Trigger(parameter.name); else controller.ResetTrigger(parameter.name);
                    break;
                }
            }
            animator.controller=std::move(controller);
        }
        animator.previous={}; animator.fadeElapsed=0; animator.fadeDuration=0; animator.controllerStarted=false;
        world->SetAnimator(entitiesByID.at(r.id),animator);
    }

    auto document=std::make_unique<SceneDocument>(std::move(world));
    document->persistentIDs_.clear();
    for(const auto &[id,e]:entitiesByID) document->persistentIDs_.emplace(e.value,id);
    if(maxID==std::numeric_limits<std::uint64_t>::max()) Fail("Persistent entity ID space exhausted");
    document->nextPersistentID_=maxID+1;
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
    std::ifstream file(path,std::ios::binary); if(!file) throw std::runtime_error("Cannot open scene file: "+path.string());
    std::string data; char chunk[8192];
    while(file) {
        file.read(chunk,sizeof(chunk)); data.append(chunk,static_cast<std::size_t>(file.gcount()));
        if(data.size()>MaxBytes) Fail("Scene exceeds size limit");
    }
    auto document=Deserialize(data,std::move(resolve));
    document->MarkSaved();
    return document;
}

} // namespace PlagC::Editor