#pragma once

#include <cstddef>

namespace octob::test
{

class ScopedAllocationTracking
{
 public:
  ScopedAllocationTracking();
  ~ScopedAllocationTracking();

  ScopedAllocationTracking(const ScopedAllocationTracking&) = delete;
  ScopedAllocationTracking& operator=(const ScopedAllocationTracking&) = delete;
  ScopedAllocationTracking(ScopedAllocationTracking&&) = delete;
  ScopedAllocationTracking& operator=(ScopedAllocationTracking&&) = delete;
};

class ScopedAllocationTrackingPause
{
 public:
  ScopedAllocationTrackingPause();
  ~ScopedAllocationTrackingPause();

  ScopedAllocationTrackingPause(const ScopedAllocationTrackingPause&) = delete;
  ScopedAllocationTrackingPause& operator=(const ScopedAllocationTrackingPause&) = delete;
  ScopedAllocationTrackingPause(ScopedAllocationTrackingPause&&) = delete;
  ScopedAllocationTrackingPause& operator=(ScopedAllocationTrackingPause&&) = delete;

 private:
  bool wasTracking_;
};

bool allocationTrackingAvailable();
size_t trackedAllocationCount();
void resetTrackedAllocationCount();

}  // namespace octob::test
