#include "octobass-core/FETCompressor.hpp"

#include <cmath>

namespace octob
{

namespace
{
// 1176-style targets at full amount: fast peak limiter
constexpr float kTargetThresholdDb = -30.0f;
constexpr float kTargetRatio = 20.0f;
constexpr float kKneeDb = 2.0f;

constexpr float kAttackMs = 0.5f;
constexpr float kReleaseMs = 35.0f;
constexpr float kHoldMs = 10.0f;

// Maximum gain reduction safety clamp
constexpr float kMaxGainReductionDb = -40.0f;

float msToCoeff(float ms, SampleRate sampleRate)
{
  if (ms <= 0.0f || sampleRate <= 0.0)
    return 0.0f;
  return std::exp(-1.0f / (static_cast<float>(sampleRate) * ms * 0.001f));
}
}  // namespace

FETCompressor::FETCompressor()
    : sampleRate_(44100.0),
      amount_(0.0f),
      thresholdDb_(0.0f),
      ratio_(1.0f),
      kneeDb_(0.0f),
      attackCoeff_(0.0f),
      releaseCoeff_(0.0f),
      envelopeDb_(-96.0f),
      gainReductionDb_(0.0f),
      holdCounter_(0.0f),
      holdTimeSamples_(0.0f)
{
  updateParameters();
}

void FETCompressor::setSampleRate(SampleRate sampleRate)
{
  sampleRate_ = sampleRate;
  updateParameters();
}

void FETCompressor::setAmount(float amount)
{
  amount_ = CompressorMode::clamp(amount, 0.0f, 1.0f);
  updateParameters();
}

void FETCompressor::process(const Sample* input, Sample* output, FrameCount numFrames)
{
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    float in = input[i];

    // Feed-forward peak detection on input
    float inputLevelDb = (std::fabs(in) > 1e-30f)
                             ? 20.0f * std::log10(std::fabs(in))
                             : -96.0f;

    // Ballistic envelope follower with peak hold to prevent sub-cycle gain ripple.
    // The hold keeps the envelope stable between peaks of the bass waveform,
    // eliminating gain modulation at the signal frequency.
    if (inputLevelDb > envelopeDb_)
    {
      envelopeDb_ = attackCoeff_ * envelopeDb_ + (1.0f - attackCoeff_) * inputLevelDb;
      holdCounter_ = holdTimeSamples_;
    }
    else if (holdCounter_ > 0.0f)
    {
      holdCounter_ -= 1.0f;
    }
    else
    {
      envelopeDb_ = releaseCoeff_ * envelopeDb_ + (1.0f - releaseCoeff_) * inputLevelDb;
    }

    // Denormal guard
    if (envelopeDb_ < -96.0f)
      envelopeDb_ = -96.0f;

    // Gain computer: soft-knee curve
    float targetDb = computeStaticCurve(envelopeDb_);
    float gainReduction = targetDb - envelopeDb_;
    gainReduction = CompressorMode::clamp(gainReduction, kMaxGainReductionDb, 0.0f);

    gainReductionDb_ = gainReduction;

    float gainLinear = CompressorMode::dbToLinear(gainReduction);
    output[i] = in * gainLinear;
  }
}

void FETCompressor::reset()
{
  envelopeDb_ = -96.0f;
  gainReductionDb_ = 0.0f;
  holdCounter_ = 0.0f;
}

float FETCompressor::getGainReductionDb() const
{
  return gainReductionDb_;
}

float FETCompressor::getStaticMakeupDb() const
{
  if (ratio_ <= 1.0f)
    return 0.0f;
  return -thresholdDb_ * (1.0f - 1.0f / ratio_) * 0.5f;
}

void FETCompressor::updateParameters()
{
  thresholdDb_ = amount_ * kTargetThresholdDb;
  ratio_ = 1.0f + amount_ * (kTargetRatio - 1.0f);
  kneeDb_ = amount_ * kKneeDb;

  attackCoeff_ = msToCoeff(kAttackMs, sampleRate_);
  releaseCoeff_ = msToCoeff(kReleaseMs, sampleRate_);
  holdTimeSamples_ = static_cast<float>(sampleRate_) * kHoldMs * 0.001f;
}

float FETCompressor::computeStaticCurve(float inputDb) const
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
