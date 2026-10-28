#pragma once

#include <vulkan/vulkan_core.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace Ogre
{
    struct PlagCVulkanPresentCompletion
    {
        bool valid = false;
        VkResult result = VK_SUCCESS;
        double hostMs = 0.0;
        double queueDelayMs = 0.0;
        double mainWaitMs = 0.0;
    };

    /** Moves vkQueuePresentKHR host-side blocking off the render/game thread.

        The worker never allows presentation and vkQueueSubmit to execute concurrently on the
        same VkQueue. VulkanQueue takes sharedGraphicsPresentMutex() around submissions when the
        graphics and presentation handles alias. The next swapchain acquire calls waitFor(), so
        presentation errors are consumed on the main thread before the image can be reused.
    */
    class PlagCVulkanPresentScheduler final
    {
    public:
        static PlagCVulkanPresentScheduler &get();

        PlagCVulkanPresentScheduler( const PlagCVulkanPresentScheduler & ) = delete;
        PlagCVulkanPresentScheduler &operator=( const PlagCVulkanPresentScheduler & ) = delete;

        void enqueue( VkQueue queue, VkSwapchainKHR swapchain, VkSemaphore waitSemaphore,
                      std::uint32_t imageIndex, bool serializeWithGraphicsQueue );

        /// Returns immediately with valid=false when this swapchain has no queued presentation.
        PlagCVulkanPresentCompletion waitFor( VkSwapchainKHR swapchain );

        /// Used by graphics submission when graphics and presentation are the same VkQueue.
        std::mutex &sharedGraphicsPresentMutex() noexcept { return mSharedGraphicsPresentMutex; }

    private:
        using Clock = std::chrono::steady_clock;

        struct Request
        {
            VkQueue queue = VK_NULL_HANDLE;
            VkSwapchainKHR swapchain = VK_NULL_HANDLE;
            VkSemaphore waitSemaphore = VK_NULL_HANDLE;
            std::uint32_t imageIndex = 0u;
            bool serializeWithGraphicsQueue = false;
            Clock::time_point enqueuedAt{};
        };

        struct Completion
        {
            VkResult result = VK_SUCCESS;
            double hostMs = 0.0;
            double queueDelayMs = 0.0;
        };

        PlagCVulkanPresentScheduler();
        ~PlagCVulkanPresentScheduler();

        void workerMain();

        std::mutex mStateMutex;
        std::condition_variable mWorkCv;
        std::condition_variable mCompletionCv;
        std::deque<Request> mRequests;
        std::map<VkSwapchainKHR, Completion> mCompleted;
        std::set<VkSwapchainKHR> mOutstanding;
        bool mStopping = false;
        std::thread mWorker;

        // Vulkan requires host access to a single VkQueue to be externally synchronized.
        std::mutex mSharedGraphicsPresentMutex;
    };
}  // namespace Ogre
