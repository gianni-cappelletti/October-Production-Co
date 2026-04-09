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
constexpr float kReleaseMs = 50.0f;

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
      smoothedGrDb_(0.0f),
      gainReductionDb_(0.0f)
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

    // Feed-forward: compute instantaneous level and gain reduction in dB.
    // No smoothing on the level — the smoother acts on the GR output instead.
    float inputLevelDb = (std::fabs(in) > 1e-30f)
                             ? 20.0f * std::log10(std::fabs(in))
                             : -96.0f;

    float targetDb = computeStaticCurve(inputLevelDb);
    float instantGrDb = targetDb - inputLevelDb;
    instantGrDb = CompressorMode::clamp(instantGrDb, kMaxGainReductionDb, 0.0f);

    // Branching smoother on the gain reduction signal (Giannoulis et al. 2012).
    // Smoothing GR instead of level prevents the gain computer's nonlinearity
    // from converting level ripple into gain modulation (distortion).
    if (instantGrDb < smoothedGrDb_)
      smoothedGrDb_ = attackCoeff_ * smoothedGrDb_ + (1.0f - attackCoeff_) * instantGrDb;
    else
      smoothedGrDb_ = releaseCoeff_ * smoothedGrDb_ + (1.0f - releaseCoeff_) * instantGrDb;

    // Denormal guard
    if (smoothedGrDb_ > -1e-8f)
      smoothedGrDb_ = 0.0f;

    gainReductionDb_ = smoothedGrDb_;

    float gainLinear = CompressorMode::dbToLinear(smoothedGrDb_);
    output[i] = in * gainLinear;
  }
}

void FETCompressor::reset()
{
  smoothedGrDb_ = 0.0f;
  gainReductionDb_ = 0.0f;
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
