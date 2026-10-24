#pragma once
#include <PlagC/Physics/PhysicsTypes.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/Body.h>
#include <map>
#include <mutex>
#include <unordered_map>
namespace PlagC {
struct JoltContacts final:JPH::ContactListener {
    using Pair=std::pair<std::uint64_t,std::uint64_t>; // Sensor, other public identity.
    struct Contact {Pair pair;bool sensor=false;};
    std::mutex mutex;
    std::unordered_map<JPH::SubShapeIDPair,Contact> contacts;
    std::map<Pair,unsigned> counts; // Retains zero-count pairs for main-thread overlap verification.
    std::map<Pair,bool> occupied;
    void OnContactAdded(const JPH::Body& a,const JPH::Body& b,const JPH::ContactManifold& m,JPH::ContactSettings&)override{
        std::lock_guard lock(mutex);
        JPH::SubShapeIDPair key(a.GetID(),m.mSubShapeID1,b.GetID(),m.mSubShapeID2);
        Contact c;c.sensor=a.IsSensor()||b.IsSensor();c.pair=a.IsSensor()?Pair{a.GetUserData(),b.GetUserData()}:Pair{b.GetUserData(),a.GetUserData()};
        if(contacts.emplace(key,c).second&&c.sensor)++counts[c.pair];
    }
    void OnContactRemoved(const JPH::SubShapeIDPair& key)override{
        std::lock_guard lock(mutex);auto it=contacts.find(key);if(it==contacts.end())return;
        if(it->second.sensor){auto found=counts.find(it->second.pair);if(found!=counts.end()&&found->second)--found->second;}
        contacts.erase(it);
    }
};
}
