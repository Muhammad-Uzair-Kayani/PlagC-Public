#pragma once
#include <PlagC/Physics/PhysicsTypes.h>
#include "PhysicsContactRegistry.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/Body.h>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
namespace PlagC {
struct JoltContacts final:JPH::ContactListener {
    using Pair=std::pair<std::uint64_t,std::uint64_t>; // Sensor/other for triggers; sorted handles for collisions.
    struct Contact {Pair pair;bool sensor=false;};
    struct Filter {CollisionLayer layer=CollisionLayer::Dynamic;CollisionLayerMask mask=AllCollisionLayers;};
    std::mutex mutex;
    std::unordered_map<JPH::SubShapeIDPair,Contact> contacts;
    std::map<Pair,unsigned> counts; // Sensor overlap counts; zero-count pairs are retained for verification.
    std::map<Pair,bool> occupied;
    mutable std::shared_mutex filterMutex;
    std::unordered_map<std::uint64_t,Filter> filters;

    void RegisterBody(std::uint64_t handle,CollisionLayer layer,CollisionLayerMask mask){
        std::unique_lock lock(filterMutex);filters[handle]={layer,mask};
    }
    void UnregisterBody(std::uint64_t handle){
        std::unique_lock lock(filterMutex);filters.erase(handle);
    }

    JPH::ValidateResult OnContactValidate(const JPH::Body& a,const JPH::Body& b,JPH::RVec3Arg,const JPH::CollideShapeResult&)override{
        std::shared_lock lock(filterMutex);
        const auto ia=filters.find(a.GetUserData()),ib=filters.find(b.GetUserData());
        if(ia==filters.end()||ib==filters.end())return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        const bool aAccepts=(ia->second.mask&CollisionLayerBit(ib->second.layer))!=0;
        const bool bAccepts=(ib->second.mask&CollisionLayerBit(ia->second.layer))!=0;
        return aAccepts&&bAccepts?JPH::ValidateResult::AcceptAllContactsForThisBodyPair:JPH::ValidateResult::RejectAllContactsForThisBodyPair;
    }

    ~JoltContacts() override {
        std::lock_guard lock(mutex);
        for(const auto& [key,contact]:contacts)
            if(!contact.sensor)PhysicsContactRegistry::Remove({contact.pair.first,contact.pair.second});
    }

    void OnContactAdded(const JPH::Body& a,const JPH::Body& b,const JPH::ContactManifold& m,JPH::ContactSettings&)override{
        std::lock_guard lock(mutex);
        JPH::SubShapeIDPair key(a.GetID(),m.mSubShapeID1,b.GetID(),m.mSubShapeID2);
        Contact c;c.sensor=a.IsSensor()||b.IsSensor();
        const auto aHandle=a.GetUserData(),bHandle=b.GetUserData();
        if(c.sensor)c.pair=a.IsSensor()?Pair{aHandle,bHandle}:Pair{bHandle,aHandle};
        else c.pair=aHandle<bHandle?Pair{aHandle,bHandle}:Pair{bHandle,aHandle};
        if(contacts.emplace(key,c).second){
            if(c.sensor)++counts[c.pair];
            else PhysicsContactRegistry::Add({c.pair.first,c.pair.second});
        }
    }
    void OnContactRemoved(const JPH::SubShapeIDPair& key)override{
        std::lock_guard lock(mutex);auto it=contacts.find(key);if(it==contacts.end())return;
        if(it->second.sensor){auto found=counts.find(it->second.pair);if(found!=counts.end()&&found->second)--found->second;}
        else PhysicsContactRegistry::Remove({it->second.pair.first,it->second.pair.second});
        contacts.erase(it);
    }
};
}
