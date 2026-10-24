#pragma once
#include <PlagC/Jobs.h>
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Physics/PhysicsSettings.h>
namespace PlagC {
// Experimental adapter: Jolt owns dependency/barrier semantics; PlagC schedules ready jobs.
// Drain after each simulation update: a completed Jolt barrier does not imply every
// queued wrapper has released its reference yet.
class JoltJobs final:public JPH::JobSystemWithBarrier {
public:
    explicit JoltJobs(PlagC::JobSystem& jobs):JPH::JobSystemWithBarrier(JPH::cMaxPhysicsBarriers),jobs_(jobs){}
    ~JoltJobs()override{Drain();}
    int GetMaxConcurrency()const override{return int(jobs_.WorkerCount()+1);}
    JobHandle CreateJob(const char* name,JPH::ColorArg color,const JobFunction& function,JPH::uint32 dependencies=0)override{
        auto* job=new Job(name,color,this,function,dependencies);JobHandle handle(job);if(!dependencies)QueueJob(job);return handle;
    }
    void Drain(){jobs_.Wait(group_);}
private:
    PlagC::JobSystem& jobs_;PlagC::JobGroup group_;
    void FreeJob(Job* job)override{delete job;}
    void QueueJob(Job* job)override{
        job->AddRef();
        auto run=[](void* value){auto* j=static_cast<Job*>(value);j->Execute();j->Release();};
        // Never block a worker waiting for queue capacity. Inline execution also
        // supports shutdown rejection while allowing Jolt's graph to finish.
        if(jobs_.TrySubmit(group_,run,job)!=SubmitResult::Accepted)run(job);
    }
    void QueueJobs(Job** jobs,JPH::uint count)override{for(JPH::uint i=0;i<count;++i)QueueJob(jobs[i]);}
};
}
