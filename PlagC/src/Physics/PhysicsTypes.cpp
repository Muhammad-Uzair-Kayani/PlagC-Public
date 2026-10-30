#include "pgcpch.h"
#include <PlagC/Physics/PhysicsTypes.h>
namespace PlagC {
namespace {void Require(bool ok){if(!ok)throw std::invalid_argument("invalid physics body configuration");}bool Finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}unsigned Index(CollisionLayer l){auto i=unsigned(l);Require(i<CollisionMatrix::Count);return i;}}
CollisionMatrix::CollisionMatrix(){for(auto& row:pairs)row.fill(true);Set(CollisionLayer::StaticWorld,CollisionLayer::StaticWorld,false);Set(CollisionLayer::Sensor,CollisionLayer::Sensor,false);Set(CollisionLayer::Sensor,CollisionLayer::StaticWorld,false);}
void CollisionMatrix::Set(CollisionLayer a,CollisionLayer b,bool on){auto i=Index(a),j=Index(b);pairs[i][j]=pairs[j][i]=on;}
bool CollisionMatrix::Get(CollisionLayer a,CollisionLayer b)const{return pairs[Index(a)][Index(b)];}
void ValidatePhysicsBody(const PhysicsBodyDesc& d){
 const auto& b=d.component.body;const auto& c=d.component.collider;const auto& t=d.transform;
 Require(b.motion==MotionType::Static||b.motion==MotionType::Dynamic||b.motion==MotionType::Kinematic);
 Index(b.layer);Require((b.collisionMask&~AllCollisionLayers)==0);
 Require(b.sensor==(b.layer==CollisionLayer::Sensor));
 Require(b.sensor || ((b.motion==MotionType::Static)==(b.layer==CollisionLayer::StaticWorld)));
 Require(std::isfinite(b.mass)&&b.mass>0&&std::isfinite(b.gravityScale)&&std::isfinite(b.linearDamping)&&b.linearDamping>=0&&std::isfinite(b.angularDamping)&&b.angularDamping>=0);
 Require(Finite(t.position)&&Finite(t.rotationDegrees)&&Finite(t.scale)&&t.scale.x>0&&t.scale.y>0&&t.scale.z>0);
 Require(Finite(c.dimensions)&&c.dimensions.x>0&&c.dimensions.y>0&&c.dimensions.z>0);
 Require(std::isfinite(c.material.friction)&&c.material.friction>=0&&std::isfinite(c.material.restitution)&&c.material.restitution>=0&&c.material.restitution<=1);
 Require(unsigned(c.type)<=unsigned(ColliderType::Heightfield));
 Require(Finite(c.centerOfMassOffset));
 if(c.type==ColliderType::Sphere||c.type==ColliderType::Capsule||c.type==ColliderType::Cylinder)Require(t.scale.x==t.scale.z);
 if(c.type==ColliderType::Sphere||c.type==ColliderType::Capsule)Require(t.scale.x==t.scale.y);
 Require(b.linearDamping<=1&&b.angularDamping<=1);
 const Vec3 size{c.dimensions.x*t.scale.x,c.dimensions.y*t.scale.y,c.dimensions.z*t.scale.z};
 Require(Finite(size)&&size.x>=.01f&&size.y>=.01f&&size.z>=.01f);
 if(unsigned(c.type)>=unsigned(ColliderType::ConvexHull)){
  Require(bool(c.geometry));const auto& g=*c.geometry;
  if(c.type==ColliderType::ConvexHull||c.type==ColliderType::Mesh){Require(g.vertices.size()>=4);for(auto v:g.vertices)Require(Finite(v));}
  if(c.type==ColliderType::Mesh){Require(b.motion==MotionType::Static&&!g.indices.empty()&&g.indices.size()%3==0);for(auto i:g.indices)Require(i<g.vertices.size());}
  if(c.type==ColliderType::Heightfield){Require(b.motion==MotionType::Static&&g.samples>=4&&g.samples<=4096&&g.samples%4==0&&g.heights.size()==std::size_t(g.samples)*g.samples);Require(Finite(g.offset)&&Finite(g.spacing)&&g.spacing.x>0&&g.spacing.y>0&&g.spacing.z>0);for(auto h:g.heights)Require(std::isfinite(h));}
  if(c.type==ColliderType::Compound){Require(!g.children.empty()&&g.children.size()<=1024);for(const auto& child:g.children){Require(unsigned(child.type)<=unsigned(ColliderType::Cylinder));auto part=d;part.component.collider={};part.component.collider.type=child.type;part.component.collider.dimensions=child.dimensions;part.transform=child.transform;ValidatePhysicsBody(part);}}
 }
}
}
