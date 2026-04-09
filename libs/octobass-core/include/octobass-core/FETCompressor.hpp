#pragma once

#include "CompressorMode.hpp"

namespace octob
{

// Feed-forward FET compressor inspired by the 1176.
// Peak detection on the input with ultra-fast time constants for aggressive, punchy compression.
// Uses a standard dB-domain gain computer with high ratio for near-limiting behavior.
class FETCompressor : public CompressorMode
{
 public:
  FETCompressor();

  void setSampleRate(SampleRate sampleRate) override;
  void setAmount(float amount) override;
  void process(const Sample* input, Sample* output, FrameCount numFrames) override;
  void reset() override;
  float getGainReductionDb() const override;
  float getStaticMakeupDb() const override;

 private:
  SampleRate sampleRate_;
  float amount_;

  // Compression parameters (interpolated from amount)
  float thresholdDb_;
  float ratio_;
  float kneeDb_;
  float attackCoeff_;
  float releaseCoeff_;

  // State: smoothed gain reduction (branching smoother on GR, not level)
  float smoothedGrDb_;
  float gainReductionDb_;

  void updateParameters();
  float computeStaticCurve(float inputDb) const;
};

}  // namespace octob
