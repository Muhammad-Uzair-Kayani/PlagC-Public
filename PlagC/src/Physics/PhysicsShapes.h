#pragma once
#include <PlagC/Physics/PhysicsTypes.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
namespace PlagC { JPH::ShapeRefC MakePhysicsShape(const ColliderComponent&,Vec3 scale); }
