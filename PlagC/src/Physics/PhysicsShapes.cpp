#include "pgcpch.h"
#include "PhysicsShapes.h"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
namespace PlagC {
namespace {
JPH::Vec3 V(Vec3 v){return {v.x,v.y,v.z};}
JPH::Quat Q(Vec3 v){constexpr float r=3.14159265358979323846f/180;return JPH::Quat::sRotation(JPH::Vec3::sAxisY(),v.y*r)*JPH::Quat::sRotation(JPH::Vec3::sAxisX(),v.x*r)*JPH::Quat::sRotation(JPH::Vec3::sAxisZ(),v.z*r);}
JPH::ShapeRefC Checked(const JPH::ShapeSettings::ShapeResult& result){if(result.HasError())throw std::invalid_argument(result.GetError().c_str());return result.Get();}
}
JPH::ShapeRefC MakePhysicsShape(const ColliderComponent& c,Vec3 scale){
    auto v=c.dimensions;JPH::ShapeRefC shape;
    switch(c.type){
    case ColliderType::Box:shape=Checked(JPH::BoxShapeSettings(V(v),std::min({.05f,v.x*.1f,v.y*.1f,v.z*.1f})).Create());break;
    case ColliderType::Sphere:shape=Checked(JPH::SphereShapeSettings(v.x).Create());break;
    case ColliderType::Capsule:shape=Checked(JPH::CapsuleShapeSettings(v.y,v.x).Create());break;
    case ColliderType::Cylinder:shape=Checked(JPH::CylinderShapeSettings(v.y,v.x,std::min({.05f,v.x*.1f,v.y*.1f})).Create());break;
    case ColliderType::ConvexHull:{
        JPH::Array<JPH::Vec3> vertices;for(auto point:c.geometry->vertices)vertices.push_back(V(point));
        shape=Checked(JPH::ConvexHullShapeSettings(vertices,.01f).Create());break;
    }
    case ColliderType::Compound:{
        JPH::StaticCompoundShapeSettings compound;
        for(const auto& child:c.geometry->children){ColliderComponent part;part.type=child.type;part.dimensions=child.dimensions;auto inner=MakePhysicsShape(part,child.transform.scale);compound.AddShape(V(child.transform.position),Q(child.transform.rotationDegrees),inner);}
        shape=Checked(compound.Create());break;
    }
    case ColliderType::Mesh:{
        JPH::VertexList vertices;for(auto p:c.geometry->vertices)vertices.emplace_back(p.x,p.y,p.z);
        JPH::IndexedTriangleList triangles;const auto& indices=c.geometry->indices;
        for(std::size_t i=0;i<indices.size();i+=3)triangles.emplace_back(indices[i],indices[i+1],indices[i+2]);
        shape=Checked(JPH::MeshShapeSettings(vertices,triangles).Create());break;
    }
    case ColliderType::Heightfield:{
        const auto& g=*c.geometry;shape=Checked(JPH::HeightFieldShapeSettings(g.heights.data(),V(g.offset),V(g.spacing),g.samples).Create());break;
    }
    }
    if(scale.x!=1||scale.y!=1||scale.z!=1)shape=Checked(JPH::ScaledShapeSettings(shape,V(scale)).Create());
    if(c.centerOfMassOffset.x!=0||c.centerOfMassOffset.y!=0||c.centerOfMassOffset.z!=0)shape=Checked(JPH::OffsetCenterOfMassShapeSettings(V(c.centerOfMassOffset),shape).Create());
    return shape;
}
}
