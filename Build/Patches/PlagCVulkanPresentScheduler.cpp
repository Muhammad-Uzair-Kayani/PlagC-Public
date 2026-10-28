#include "PlagCVulkanPresentScheduler.h"

#include <cassert>
#include <utility>

namespace Ogre
{
    PlagCVulkanPresentScheduler &PlagCVulkanPresentScheduler::get()
    {
        static PlagCVulkanPresentScheduler scheduler;
        return scheduler;
    }

    PlagCVulkanPresentScheduler::PlagCVulkanPresentScheduler() :
        mWorker( &PlagCVulkanPresentScheduler::workerMain, this )
    {
    }

    PlagCVulkanPresentScheduler::~PlagCVulkanPresentScheduler()
    {
        {
            std::lock_guard<std::mutex> lock( mStateMutex );
            mStopping = true;
        }
        mWorkCv.notify_all();
        if( mWorker.joinable() )
            mWorker.join();
    }

    void PlagCVulkanPresentScheduler::enqueue( VkQueue queue, VkSwapchainKHR swapchain,
                                                VkSemaphore waitSemaphore,
                                                std::uint32_t imageIndex,
                                                bool serializeWithGraphicsQueue )
    {
        assert( queue != VK_NULL_HANDLE );
        assert( swapchain != VK_NULL_HANDLE );

        Request request;
        request.queue = queue;
        request.swapchain = swapchain;
        request.waitSemaphore = waitSemaphore;
        request.imageIndex = imageIndex;
        request.serializeWithGraphicsQueue = serializeWithGraphicsQueue;
        request.enqueuedAt = Clock::now();

        {
            std::lock_guard<std::mutex> lock( mStateMutex );
            // A swapchain cannot be presented twice before it is acquired again. If this fires,
            // the window state machine changed and silently queueing another request would be unsafe.
            assert( mOutstanding.find( swapchain ) == mOutstanding.end() );
            mOutstanding.insert( swapchain );
            mRequests.push_back( request );
        }
        mWorkCv.notify_one();
    }

    PlagCVulkanPresentCompletion PlagCVulkanPresentScheduler::waitFor( VkSwapchainKHR swapchain )
    {
        PlagCVulkanPresentCompletion out;
        if( swapchain == VK_NULL_HANDLE )
            return out;

        const Clock::time_point waitStart = Clock::now();
        std::unique_lock<std::mutex> lock( mStateMutex );
        if( mOutstanding.find( swapchain ) == mOutstanding.end() )
            return out;

        mCompletionCv.wait( lock, [this, swapchain] {
            return mCompleted.find( swapchain ) != mCompleted.end();
        } );

        const Completion completion = mCompleted.find( swapchain )->second;
        mCompleted.erase( swapchain );
        mOutstanding.erase( swapchain );
        lock.unlock();

        out.valid = true;
        out.result = completion.result;
        out.hostMs = completion.hostMs;
        out.queueDelayMs = completion.queueDelayMs;
        out.mainWaitMs =
            std::chrono::duration<double, std::milli>( Clock::now() - waitStart ).count();
        return out;
    }

    void PlagCVulkanPresentScheduler::workerMain()
    {
        for( ;; )
        {
            Request request;
            {
                std::unique_lock<std::mutex> lock( mStateMutex );
                mWorkCv.wait( lock, [this] { return mStopping || !mRequests.empty(); } );
                if( mStopping && mRequests.empty() )
                    return;
                request = mRequests.front();
                mRequests.pop_front();
            }

            const Clock::time_point beginPresent = Clock::now();
            const double queueDelayMs =
                std::chrono::duration<double, std::milli>( beginPresent - request.enqueuedAt ).count();

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.swapchainCount = 1u;
            presentInfo.pSwapchains = &request.swapchain;
            presentInfo.pImageIndices = &request.imageIndex;
            if( request.waitSemaphore != VK_NULL_HANDLE )
            {
                presentInfo.waitSemaphoreCount = 1u;
                presentInfo.pWaitSemaphores = &request.waitSemaphore;
            }

            VkResult result = VK_SUCCESS;
            const Clock::time_point hostStart = Clock::now();
            if( request.serializeWithGraphicsQueue )
            {
                std::lock_guard<std::mutex> queueLock( mSharedGraphicsPresentMutex );
                result = vkQueuePresentKHR( request.queue, &presentInfo );
            }
            else
            {
                result = vkQueuePresentKHR( request.queue, &presentInfo );
            }
            const double hostMs =
                std::chrono::duration<double, std::milli>( Clock::now() - hostStart ).count();

            {
                std::lock_guard<std::mutex> lock( mStateMutex );
                mCompleted[request.swapchain] = Completion{ result, hostMs, queueDelayMs };
            }
            mCompletionCv.notify_all();
        }
    }
}  // namespace Ogre
