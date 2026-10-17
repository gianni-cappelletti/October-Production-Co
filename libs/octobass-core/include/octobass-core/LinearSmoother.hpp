#pragma once

#include <algorithm>
#include <cmath>

#include "Types.hpp"

namespace octob
{

class LinearSmoother
{
 public:
  explicit LinearSmoother(float initialValue) : current_(initialValue), target_(initialValue) {}

  void setRampLength(SampleRate sampleRate, float rampSeconds)
  {
    rampSamples_ = std::max(1L, std::lround(sampleRate * static_cast<double>(rampSeconds)));
  }

  void setTarget(float target)
  {
    if (target == target_)
      return;
    target_ = target;
    remainingSamples_ = rampSamples_;
    step_ = (target_ - current_) / static_cast<float>(rampSamples_);
  }

  void snapToTarget()
  {
    current_ = target_;
    remainingSamples_ = 0;
  }

  bool isRamping() const { return remainingSamples_ > 0; }
  float target() const { return target_; }

  float next()
  {
    if (remainingSamples_ > 0)
      current_ = --remainingSamples_ == 0 ? target_ : current_ + step_;
    return current_;
  }

 private:
  float current_;
  float target_;
  float step_ = 0.0f;
  long rampSamples_ = 1;
  long remainingSamples_ = 0;
};

}  // namespace octob
