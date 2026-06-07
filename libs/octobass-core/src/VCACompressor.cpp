#include "octobass-core/VCACompressor.hpp"

#include <algorithm>
#include <cmath>

namespace octob
{

namespace
{
// dbx 160-style targets at full amount
constexpr float kTargetThresholdDb = -36.0f;
constexpr float kTargetRatio = 12.0f;
constexpr float kKneeDb = 3.0f;

constexpr float kAttackMs = 1.0f;
constexpr float kFastReleaseMs = 40.0f;
constexpr float kSlowReleaseMs = 150.0f;
constexpr float kRmsWindowMs = 10.0f;

float msToCoeff(float ms, SampleRate sampleRate)
{
  if (ms <= 0.0f || sampleRate <= 0.0)
    return 0.0f;
  return std::exp(-1.0f / (static_cast<float>(sampleRate) * ms * 0.001f));
}
}  // namespace

VCACompressor::VCACompressor()
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

void VCACompressor::setSampleRate(SampleRate sampleRate)
{
  sampleRate_ = sampleRate;
  updateParameters();
}

void VCACompressor::setAmount(float amount)
{
  amount_ = std::clamp(amount, 0.0f, 1.0f);
  updateParameters();
}

void VCACompressor::process(const Sample* input, Sample* output, FrameCount numFrames)
{
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    float in = input[i];

    // RMS level detection (single-pole IIR on squared signal)
    double inSquared = static_cast<double>(in) * in;
    rmsSquared_ = static_cast<double>(rmsCoeff_) * rmsSquared_ +
                  (1.0 - static_cast<double>(rmsCoeff_)) * inSquared;

    float levelDb = (rmsSquared_ > 1e-30)
                        ? std::log2(static_cast<float>(rmsSquared_)) * (Log2ToDb * 0.5f)
                        : -96.0f;

    // Gain computer (soft-knee curve)
    float targetDb = computeStaticCurve(levelDb);
    float gainReduction = targetDb - levelDb;

    // Ballistic envelope: attack for increasing reduction, dual-release for decreasing
    if (gainReduction < envelopeDb_)
    {
      envelopeDb_ = attackCoeff_ * envelopeDb_ + (1.0f - attackCoeff_) * gainReduction;
    }
    else
    {
      // Dual release: blend fast and slow based on how deep the gain reduction is
      float blend = std::clamp(-envelopeDb_ / 20.0f, 0.0f, 1.0f);
      float releaseCoeff = fastReleaseCoeff_ + blend * (slowReleaseCoeff_ - fastReleaseCoeff_);
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

void VCACompressor::reset()
{
  rmsSquared_ = 0.0;
  envelopeDb_ = 0.0f;
  gainReductionDb_ = 0.0f;
}

float VCACompressor::getGainReductionDb() const
{
  return gainReductionDb_;
}

float VCACompressor::getStaticMakeupDb() const
{
  if (ratio_ <= 1.0f)
    return 0.0f;
  return -thresholdDb_ * (1.0f - 1.0f / ratio_) * 0.5f;
}

void VCACompressor::updateParameters()
{
  // Interpolate from neutral (amount=0) to targets (amount=1)
  thresholdDb_ = amount_ * kTargetThresholdDb;
  ratio_ = 1.0f + amount_ * (kTargetRatio - 1.0f);
  kneeDb_ = amount_ * kKneeDb;

  attackCoeff_ = msToCoeff(kAttackMs, sampleRate_);
  fastReleaseCoeff_ = msToCoeff(kFastReleaseMs, sampleRate_);
  slowReleaseCoeff_ = msToCoeff(kSlowReleaseMs, sampleRate_);
  rmsCoeff_ = msToCoeff(kRmsWindowMs, sampleRate_);
}

float VCACompressor::computeStaticCurve(float inputDb) const
{
  if (ratio_ <= 1.0f)
    return inputDb;

  float halfKnee = kneeDb_ * 0.5f;

  if (kneeDb_ > 0.0f && inputDb >= (thresholdDb_ - halfKnee) &&
      inputDb <= (thresholdDb_ + halfKnee))
  {
    // Soft knee region: quadratic interpolation
    float x = inputDb - thresholdDb_ + halfKnee;
    return inputDb + (1.0f / ratio_ - 1.0f) * x * x / (2.0f * kneeDb_);
  }
  else if (inputDb > thresholdDb_ + halfKnee)
  {
    // Above knee: full ratio compression
    return thresholdDb_ + (inputDb - thresholdDb_) / ratio_;
  }

  // Below knee: no compression
  return inputDb;
}

}  // namespace octob
