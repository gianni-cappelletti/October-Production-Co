#include "AllocationTracker.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

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

void recordAllocation()
{
  if (trackingEnabled.load(std::memory_order_relaxed))
    allocationCount.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

#if defined(OCTOB_TRACK_WITH_SANITIZER_HOOKS)

namespace
{

void onSanitizerMalloc(const volatile void* /*memory*/, size_t /*size*/)
{
  recordAllocation();
}

void onSanitizerFree(const volatile void* /*memory*/) {}

const int sanitizerHooksInstalled =
    __sanitizer_install_malloc_and_free_hooks(&onSanitizerMalloc, &onSanitizerFree);

}  // namespace

#else

namespace
{

void* allocate(std::size_t size)
{
  recordAllocation();
  if (void* memory = std::malloc(size == 0 ? 1 : size))
    return memory;
  throw std::bad_alloc();
}

void* allocateAligned(std::size_t size, std::align_val_t alignment)
{
  recordAllocation();
  const auto align = static_cast<std::size_t>(alignment);
  const std::size_t roundedSize = ((size == 0 ? 1 : size) + align - 1) / align * align;
  if (void* memory = std::aligned_alloc(align, roundedSize))
    return memory;
  throw std::bad_alloc();
}

}  // namespace

// NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)
void* operator new(std::size_t size)
{
  return allocate(size);
}

void* operator new[](std::size_t size)
{
  return allocate(size);
}

void* operator new(std::size_t size, std::align_val_t alignment)
{
  return allocateAligned(size, alignment);
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
  return allocateAligned(size, alignment);
}

void operator delete(void* memory) noexcept
{
  std::free(memory);
}

void operator delete[](void* memory) noexcept
{
  std::free(memory);
}

void operator delete(void* memory, std::size_t /*size*/) noexcept
{
  std::free(memory);
}

void operator delete[](void* memory, std::size_t /*size*/) noexcept
{
  std::free(memory);
}

void operator delete(void* memory, std::align_val_t /*alignment*/) noexcept
{
  std::free(memory);
}

void operator delete[](void* memory, std::align_val_t /*alignment*/) noexcept
{
  std::free(memory);
}

void operator delete(void* memory, std::size_t /*size*/, std::align_val_t /*alignment*/) noexcept
{
  std::free(memory);
}

void operator delete[](void* memory, std::size_t /*size*/, std::align_val_t /*alignment*/) noexcept
{
  std::free(memory);
}
// NOLINTEND(readability-inconsistent-declaration-parameter-name)

#endif

namespace octob::test
{

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
