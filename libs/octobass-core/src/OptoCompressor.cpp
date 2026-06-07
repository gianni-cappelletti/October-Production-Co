#include "octobass-core/OptoCompressor.hpp"

#include <algorithm>
#include <cmath>

namespace octob
{

namespace
{
// T4 cell time constants
constexpr float kFastAttackMs = 5.0f;
constexpr float kSlowAttackMs = 30.0f;
constexpr float kChargeRateMs = 200.0f;
constexpr float kDischargeRateMs = 2000.0f;

// Maximum gain reduction to prevent runaway in feedback loop
constexpr float kMaxGainReductionDb = -40.0f;

float msToCoeff(float ms, SampleRate sampleRate)
{
  if (ms <= 0.0f || sampleRate <= 0.0)
    return 0.0f;
  return std::exp(-1.0f / (static_cast<float>(sampleRate) * ms * 0.001f));
}
}  // namespace

OptoCompressor::OptoCompressor()
    : sampleRate_(44100.0),
      amount_(0.0f),
      fastState_(0.0f),
      slowState_(0.0f),
      charge_(0.0f),
      fastAttackCoeff_(0.0f),
      slowAttackCoeff_(0.0f),
      chargeRate_(0.0f),
      dischargeRate_(0.0f),
      gainReductionDb_(0.0f)
{
  updateCoefficients();
}

void OptoCompressor::setSampleRate(SampleRate sampleRate)
{
  sampleRate_ = sampleRate;
  updateCoefficients();
}

void OptoCompressor::setAmount(float amount)
{
  amount_ = std::clamp(amount, 0.0f, 1.0f);
}

void OptoCompressor::process(const Sample* input, Sample* output, FrameCount numFrames)
{
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    float in = input[i];

    // Feed-forward: detect on input, drive the T4 cell from the input level
    float detectedLevel = std::fabs(in);
    float drive = detectedLevel * amount_ * 6.0f;

    // Fast state: responds to transients (~10ms)
    fastState_ = fastAttackCoeff_ * fastState_ + (1.0f - fastAttackCoeff_) * drive;

    // Slow state: follows the fast state with additional smoothing (~60ms)
    slowState_ = slowAttackCoeff_ * slowState_ + (1.0f - slowAttackCoeff_) * fastState_;

    // Program-dependent release via charge accumulation
    // Charge builds up during compression, causing slower release on sustained signals
    bool isCompressing = slowState_ > 0.01f;
    if (isCompressing)
    {
      charge_ += (slowState_ - charge_) * (1.0f - chargeRate_);
    }
    else
    {
      float effectiveDischargeRate = dischargeRate_ * (1.0f - 0.5f * charge_);
      charge_ *= effectiveDischargeRate;
    }

    // Denormal guards
    if (std::fabs(fastState_) < 1e-8f)
      fastState_ = 0.0f;
    if (std::fabs(slowState_) < 1e-8f)
      slowState_ = 0.0f;
    if (std::fabs(charge_) < 1e-8f)
      charge_ = 0.0f;

    // Gain reduction derived from the T4 cell slow state
    float reductionLinear = 1.0f / (1.0f + slowState_ * 4.0f);
    gainReductionDb_ = std::clamp(linearToDb(reductionLinear), kMaxGainReductionDb, 0.0f);

    float gainLinear = dbToLinear(gainReductionDb_);
    output[i] = in * gainLinear;
  }
}

void OptoCompressor::reset()
{
  fastState_ = 0.0f;
  slowState_ = 0.0f;
  charge_ = 0.0f;
  gainReductionDb_ = 0.0f;
}

float OptoCompressor::getGainReductionDb() const
{
  return gainReductionDb_;
}

float OptoCompressor::getStaticMakeupDb() const
{
  // Opto has no explicit threshold/ratio. Estimate from the T4 cell response
  // at a nominal reference level (~0.3 amplitude, typical bass RMS).
  constexpr float kNominalLevel = 0.3f;
  constexpr float kDriveScale = 6.0f;
  constexpr float kCurveScale = 4.0f;

  float nominalDrive = kNominalLevel * amount_ * kDriveScale;
  float nominalReduction = 1.0f / (1.0f + nominalDrive * kCurveScale);
  float grDb = linearToDb(nominalReduction);
  return -grDb * 0.5f;
}

void OptoCompressor::updateCoefficients()
{
  fastAttackCoeff_ = msToCoeff(kFastAttackMs, sampleRate_);
  slowAttackCoeff_ = msToCoeff(kSlowAttackMs, sampleRate_);
  chargeRate_ = msToCoeff(kChargeRateMs, sampleRate_);
  dischargeRate_ = msToCoeff(kDischargeRateMs, sampleRate_);
}

}  // namespace octob
