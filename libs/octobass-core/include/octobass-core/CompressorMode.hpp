#pragma once

#include <algorithm>
#include <cmath>

#include "Types.hpp"

namespace octob
{

class CompressorMode
{
 public:
  virtual ~CompressorMode() = default;

  virtual void setSampleRate(SampleRate sampleRate) = 0;
  virtual void setAmount(float amount) = 0;
  virtual void process(const Sample* input, Sample* output, FrameCount numFrames) = 0;
  virtual void reset() = 0;
  virtual float getGainReductionDb() const = 0;

  // Static makeup gain derived purely from current parameters (no signal tracking).
  // Formula: -threshold * (1 - 1/ratio) / 2  for threshold/ratio modes.
  virtual float getStaticMakeupDb() const = 0;

 protected:
  // log2/exp2-based conversions: on ARM, log2f/exp2f map more directly to
  // hardware than log10f/powf, avoiding the extra multiply inside the library.
  static constexpr float kLog2ToDb = 6.02059991f;     // 20 * log10(2)
  static constexpr float kDbToLog2 = 0.16609640474f;  // 1 / (20 * log10(2))

  static float linearToDb(float linear)
  {
    if (linear < 1e-30f)
      return -96.0f;
    return std::log2(linear) * kLog2ToDb;
  }

  static float dbToLinear(float db) { return std::exp2(db * kDbToLog2); }

  static float clamp(float value, float minVal, float maxVal)
  {
    return std::max(minVal, std::min(maxVal, value));
  }
};

}  // namespace octob
