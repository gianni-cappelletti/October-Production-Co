#include "octobass-core/BusCompressor.hpp"

#include <algorithm>
#include <cmath>

namespace octob
{

namespace
{
// SSL G-bus style targets at full amount
constexpr float kTargetThresholdDb = -30.0f;
constexpr float kTargetRatio = 8.0f;
constexpr float kKneeDb = 6.0f;

constexpr float kAttackMs = 3.0f;
constexpr float kFastReleaseMs = 80.0f;
constexpr float kSlowReleaseMs = 400.0f;
constexpr float kRmsWindowMs = 12.0f;

float msToCoeff(float ms, SampleRate sampleRate)
{
  if (ms <= 0.0f || sampleRate <= 0.0)
    return 0.0f;
  return std::exp(-1.0f / (static_cast<float>(sampleRate) * ms * 0.001f));
}
}  // namespace

BusCompressor::BusCompressor()
    : sampleRate_(44100.0),
      amount_(0.0f),
      thresholdDb_(0.0f),
      ratio_(1.0f),
      kneeDb_(kKneeDb),
      attackCoeff_(0.0f),
      fastReleaseCoeff_(0.0f),
      slowReleaseCoeff_(0.0f),
      rmsCoeff_(0.0f),
      rmsSquared_(0.0),
      envelopeDb_(0.0f),
      gainReductionDb_(0.0f)
{
  updateParameters();
}

void BusCompressor::setSampleRate(SampleRate sampleRate)
{
  sampleRate_ = sampleRate;
  updateParameters();
}

void BusCompressor::setAmount(float amount)
{
  amount_ = std::clamp(amount, 0.0f, 1.0f);
  updateParameters();
}

void BusCompressor::process(const Sample* input, Sample* output, FrameCount numFrames)
{
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    float in = input[i];

    // RMS level detection
    double inSquared = static_cast<double>(in) * in;
    rmsSquared_ = static_cast<double>(rmsCoeff_) * rmsSquared_ +
                  (1.0 - static_cast<double>(rmsCoeff_)) * inSquared;

    float levelDb = (rmsSquared_ > 1e-30)
                        ? std::log2(static_cast<float>(rmsSquared_)) * (Log2ToDb * 0.5f)
                        : -96.0f;

    // Gain computer (soft-knee curve)
    float targetDb = computeStaticCurve(levelDb);
    float gainReduction = targetDb - levelDb;

    // Ballistic envelope with auto-release
    if (gainReduction < envelopeDb_)
    {
      // Attack
      envelopeDb_ = attackCoeff_ * envelopeDb_ + (1.0f - attackCoeff_) * gainReduction;
    }
    else
    {
      // Auto-release: blend fast/slow based on gain reduction depth
      float grDepth = std::clamp(-envelopeDb_ / 12.0f, 0.0f, 1.0f);
      float releaseCoeff = fastReleaseCoeff_ + grDepth * (slowReleaseCoeff_ - fastReleaseCoeff_);
      envelopeDb_ = releaseCoeff * envelopeDb_ + (1.0f - releaseCoeff) * gainReduction;
    }

    // Denormal guard
    if (std::fabs(envelopeDb_) < 1e-8f)
      envelopeDb_ = 0.0f;

    gainReductionDb_ = envelopeDb_;

    float gainLinear = dbToLinear(envelopeDb_);
    output[i] = in * gainLinear;
  }
}

void BusCompressor::reset()
{
  rmsSquared_ = 0.0;
  envelopeDb_ = 0.0f;
  gainReductionDb_ = 0.0f;
}

float BusCompressor::getGainReductionDb() const
{
  return gainReductionDb_;
}

float BusCompressor::getStaticMakeupDb() const
{
  if (ratio_ <= 1.0f)
    return 0.0f;
  return -thresholdDb_ * (1.0f - 1.0f / ratio_) * 0.5f;
}

void BusCompressor::updateParameters()
{
  thresholdDb_ = amount_ * kTargetThresholdDb;
  ratio_ = 1.0f + amount_ * (kTargetRatio - 1.0f);
  kneeDb_ = amount_ * kKneeDb;

  attackCoeff_ = msToCoeff(kAttackMs, sampleRate_);
  fastReleaseCoeff_ = msToCoeff(kFastReleaseMs, sampleRate_);
  slowReleaseCoeff_ = msToCoeff(kSlowReleaseMs, sampleRate_);
  rmsCoeff_ = msToCoeff(kRmsWindowMs, sampleRate_);
}

float BusCompressor::computeStaticCurve(float inputDb) const
{
  if (ratio_ <= 1.0f)
    return inputDb;

  float halfKnee = kneeDb_ * 0.5f;

  if (kneeDb_ > 0.0f && inputDb >= (thresholdDb_ - halfKnee) &&
      inputDb <= (thresholdDb_ + halfKnee))
  {
    float x = inputDb - thresholdDb_ + halfKnee;
    return inputDb + (1.0f / ratio_ - 1.0f) * x * x / (2.0f * kneeDb_);
  }
  else if (inputDb > thresholdDb_ + halfKnee)
  {
    return thresholdDb_ + (inputDb - thresholdDb_) / ratio_;
  }

  return inputDb;
}

}  // namespace octob
