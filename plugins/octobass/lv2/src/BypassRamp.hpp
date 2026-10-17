#pragma once

#include <algorithm>
#include <cstddef>

namespace octob::lv2
{

class BypassRamp
{
 public:
  void prepare(double sampleRate, float rampSeconds)
  {
    step_ = static_cast<float>(1.0 / (sampleRate * static_cast<double>(rampSeconds)));
  }

  void setEnabled(bool enabled) { target_ = enabled ? 1.0f : 0.0f; }

  void snapToTarget() { gain_ = target_; }

  void process(const float* dry, float* processed, size_t frames)
  {
    if (gain_ == target_)
    {
      if (target_ == 0.0f)
        std::copy_n(dry, frames, processed);
      return;
    }

    for (size_t i = 0; i < frames; ++i)
    {
      gain_ = target_ > gain_ ? std::min(target_, gain_ + step_) : std::max(target_, gain_ - step_);
      processed[i] = dry[i] + (processed[i] - dry[i]) * gain_;
    }
  }

 private:
  float gain_ = 1.0f;
  float target_ = 1.0f;
  float step_ = 1.0f;
};

}  // namespace octob::lv2
