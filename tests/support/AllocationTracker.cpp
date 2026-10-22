#include "AllocationTracker.hpp"

#include <atomic>

#if defined(__SANITIZE_ADDRESS__)
#define OCTOB_TRACK_WITH_SANITIZER_HOOKS 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define OCTOB_TRACK_WITH_SANITIZER_HOOKS 1
#endif
#endif

#if defined(OCTOB_TRACK_WITH_SANITIZER_HOOKS)
extern "C" int __sanitizer_install_malloc_and_free_hooks(void (*mallocHook)(const volatile void*,
                                                                            size_t),
                                                         void (*freeHook)(const volatile void*));
#endif

namespace
{

std::atomic<bool> trackingEnabled{false};
std::atomic<size_t> allocationCount{0};

#if defined(OCTOB_TRACK_WITH_SANITIZER_HOOKS)

void onSanitizerMalloc(const volatile void* /*memory*/, size_t /*size*/)
{
  if (trackingEnabled.load(std::memory_order_relaxed))
    allocationCount.fetch_add(1, std::memory_order_relaxed);
}

void onSanitizerFree(const volatile void* /*memory*/) {}

const bool sanitizerHooksInstalled =
    __sanitizer_install_malloc_and_free_hooks(&onSanitizerMalloc, &onSanitizerFree) != 0;

#else

const bool sanitizerHooksInstalled = false;

#endif

}  // namespace

namespace octob::test
{

bool allocationTrackingAvailable()
{
  return sanitizerHooksInstalled;
}

ScopedAllocationTracking::ScopedAllocationTracking()
{
  trackingEnabled = true;
}

ScopedAllocationTracking::~ScopedAllocationTracking()
{
  trackingEnabled = false;
}

ScopedAllocationTrackingPause::ScopedAllocationTrackingPause()
    : wasTracking_(trackingEnabled.load(std::memory_order_relaxed))
{
  trackingEnabled = false;
}

ScopedAllocationTrackingPause::~ScopedAllocationTrackingPause()
{
  trackingEnabled = wasTracking_;
}

size_t trackedAllocationCount()
{
  return allocationCount.load(std::memory_order_relaxed);
}

void resetTrackedAllocationCount()
{
  allocationCount.store(0, std::memory_order_relaxed);
}

}  // namespace octob::test
