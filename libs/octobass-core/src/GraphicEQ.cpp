#include "octobass-core/GraphicEQ.hpp"

#include <algorithm>
#include <cmath>

namespace octob
{

namespace
{

struct PeakingCoeffsD
{
  double b0;
  double b1;
  double b2;
  double a1;
  double a2;
};

// The frequency range tops out at 20 kHz, which can exceed Nyquist at lower
// sample rates; clamp so the biquad coefficients stay stable
double clampBelowNyquist(double freqHz, SampleRate sampleRate)
{
  return std::min(freqHz, 0.49 * sampleRate);
}

PeakingCoeffsD computePeakingCoeffs(double gainDb, double centerFreqHz, double q,
                                    SampleRate sampleRate)
{
  // RBJ Audio EQ Cookbook -- peaking EQ
  const double pi = 3.14159265358979323846;
  double A = std::pow(10.0, gainDb / 40.0);
  double w0 = 2.0 * pi * clampBelowNyquist(centerFreqHz, sampleRate) / sampleRate;
  double alpha = std::sin(w0) / (2.0 * q);
  double cosw0 = std::cos(w0);

  double a0 = 1.0 + alpha / A;
  double invA0 = 1.0 / a0;
  return {(1.0 + alpha * A) * invA0, (-2.0 * cosw0) * invA0, (1.0 - alpha * A) * invA0,
          (-2.0 * cosw0) * invA0, (1.0 - alpha / A) * invA0};
}

PeakingCoeffsD computeHighpassCoeffs(double cutoffFreqHz, double q, SampleRate sampleRate)
{
  // RBJ Audio EQ Cookbook -- highpass
  const double pi = 3.14159265358979323846;
  double w0 = 2.0 * pi * clampBelowNyquist(cutoffFreqHz, sampleRate) / sampleRate;
  double alpha = std::sin(w0) / (2.0 * q);
  double cosw0 = std::cos(w0);

  double invA0 = 1.0 / (1.0 + alpha);
  double b0 = ((1.0 + cosw0) / 2.0) * invA0;
  return {b0, -(1.0 + cosw0) * invA0, b0, (-2.0 * cosw0) * invA0, (1.0 - alpha) * invA0};
}

PeakingCoeffsD computeLowpassCoeffs(double cutoffFreqHz, double q, SampleRate sampleRate)
{
  // RBJ Audio EQ Cookbook -- lowpass
  const double pi = 3.14159265358979323846;
  double w0 = 2.0 * pi * clampBelowNyquist(cutoffFreqHz, sampleRate) / sampleRate;
  double alpha = std::sin(w0) / (2.0 * q);
  double cosw0 = std::cos(w0);

  double invA0 = 1.0 / (1.0 + alpha);
  double b0 = ((1.0 - cosw0) / 2.0) * invA0;
  return {b0, (1.0 - cosw0) * invA0, b0, (-2.0 * cosw0) * invA0, (1.0 - alpha) * invA0};
}

// Multiply the running magnitude-squared by |H(e^jw)|^2 of one biquad,
// evaluated with complex arithmetic:
//   N = b0 + b1*e^(-jw) + b2*e^(-2jw)
//   D = 1  + a1*e^(-jw) + a2*e^(-2jw)
void accumulateMagnitudeSq(const PeakingCoeffsD& c, double cosw, double sinw, double cos2w,
                           double sin2w, double& totalMagSq)
{
  double numRe = c.b0 + c.b1 * cosw + c.b2 * cos2w;
  double numIm = -(c.b1 * sinw + c.b2 * sin2w);
  double denRe = 1.0 + c.a1 * cosw + c.a2 * cos2w;
  double denIm = -(c.a1 * sinw + c.a2 * sin2w);

  double numMagSq = numRe * numRe + numIm * numIm;
  double denMagSq = denRe * denRe + denIm * denIm;

  if (denMagSq > 1e-30)
    totalMagSq *= numMagSq / denMagSq;
}

}  // namespace

GraphicEQ::GraphicEQ()
{
  freqsHz_.fill(DefaultGraphicEQFreqHz);
  gainsDb_.fill(DefaultGraphicEQGainDb);
}

void GraphicEQ::setSampleRate(SampleRate sampleRate)
{
  if (sampleRate > 0.0)
  {
    sampleRate_ = sampleRate;
    for (int i = 0; i < kGraphicEQNumNodes; ++i)
      updateCoefficients(i);
    updateLowCutCoefficients();
    updateHighCutCoefficients();
  }
}

bool GraphicEQ::isValidSlot(int slot)
{
  return slot >= 0 && slot < kGraphicEQNumNodes;
}

void GraphicEQ::setNodeActive(int slot, bool active)
{
  if (!isValidSlot(slot))
    return;

  if (active_[static_cast<size_t>(slot)] == active)
    return;

  active_[static_cast<size_t>(slot)] = active;
  updateCoefficients(slot);
}

bool GraphicEQ::getNodeActive(int slot) const
{
  if (!isValidSlot(slot))
    return false;

  return active_[static_cast<size_t>(slot)];
}

void GraphicEQ::setNodeFrequency(int slot, float freqHz)
{
  if (!isValidSlot(slot))
    return;

  freqHz = std::max(MinGraphicEQFreqHz, std::min(MaxGraphicEQFreqHz, freqHz));

  if (freqsHz_[static_cast<size_t>(slot)] == freqHz)
    return;

  freqsHz_[static_cast<size_t>(slot)] = freqHz;
  updateCoefficients(slot);
}

float GraphicEQ::getNodeFrequency(int slot) const
{
  if (!isValidSlot(slot))
    return DefaultGraphicEQFreqHz;

  return freqsHz_[static_cast<size_t>(slot)];
}

void GraphicEQ::setNodeGain(int slot, float gainDb)
{
  if (!isValidSlot(slot))
    return;

  gainDb = std::max(MinGraphicEQGainDb, std::min(MaxGraphicEQGainDb, gainDb));

  if (gainsDb_[static_cast<size_t>(slot)] == gainDb)
    return;

  gainsDb_[static_cast<size_t>(slot)] = gainDb;
  updateCoefficients(slot);
}

float GraphicEQ::getNodeGain(int slot) const
{
  if (!isValidSlot(slot))
    return DefaultGraphicEQGainDb;

  return gainsDb_[static_cast<size_t>(slot)];
}

void GraphicEQ::setNode(int slot, bool active, float freqHz, float gainDb)
{
  if (!isValidSlot(slot))
    return;

  auto idx = static_cast<size_t>(slot);
  freqHz = std::max(MinGraphicEQFreqHz, std::min(MaxGraphicEQFreqHz, freqHz));
  gainDb = std::max(MinGraphicEQGainDb, std::min(MaxGraphicEQGainDb, gainDb));

  if (active_[idx] == active && freqsHz_[idx] == freqHz && gainsDb_[idx] == gainDb)
    return;

  active_[idx] = active;
  freqsHz_[idx] = freqHz;
  gainsDb_[idx] = gainDb;
  updateCoefficients(slot);
}

void GraphicEQ::setLowCut(bool active, float freqHz)
{
  freqHz = std::max(MinGraphicEQFreqHz, std::min(MaxGraphicEQFreqHz, freqHz));

  if (lowCutActive_ == active && lowCutFreqHz_ == freqHz)
    return;

  lowCutActive_ = active;
  lowCutFreqHz_ = freqHz;
  updateLowCutCoefficients();
}

bool GraphicEQ::getLowCutActive() const
{
  return lowCutActive_;
}

float GraphicEQ::getLowCutFrequency() const
{
  return lowCutFreqHz_;
}

void GraphicEQ::setHighCut(bool active, float freqHz)
{
  freqHz = std::max(MinGraphicEQFreqHz, std::min(MaxGraphicEQFreqHz, freqHz));

  if (highCutActive_ == active && highCutFreqHz_ == freqHz)
    return;

  highCutActive_ = active;
  highCutFreqHz_ = freqHz;
  updateHighCutCoefficients();
}

bool GraphicEQ::getHighCutActive() const
{
  return highCutActive_;
}

float GraphicEQ::getHighCutFrequency() const
{
  return highCutFreqHz_;
}

void GraphicEQ::process(const Sample* input, Sample* output, FrameCount numFrames)
{
  if (numFrames == 0)
    return;

  if (activeNodeMask_ == 0 && !lowCutActive_ && !highCutActive_)
  {
    if (input != output)
      std::memcpy(output, input, numFrames * sizeof(Sample));
    return;
  }

  if (input != output)
    std::memcpy(output, input, numFrames * sizeof(Sample));

  for (int n = 0; n < kGraphicEQNumNodes; ++n)
  {
    if (!(activeNodeMask_ & (1u << n)))
      continue;

    auto& coeffs = coeffs_[static_cast<size_t>(n)];
    auto& state = states_[static_cast<size_t>(n)];
    for (FrameCount i = 0; i < numFrames; ++i)
      output[i] = tick(coeffs, state, output[i]);
  }

  if (lowCutActive_)
  {
    for (int stage = 0; stage < kNumCutStages; ++stage)
    {
      auto& coeffs = lowCutCoeffs_[static_cast<size_t>(stage)];
      auto& state = lowCutStates_[static_cast<size_t>(stage)];
      for (FrameCount i = 0; i < numFrames; ++i)
        output[i] = tick(coeffs, state, output[i]);
    }
  }

  if (highCutActive_)
  {
    for (int stage = 0; stage < kNumCutStages; ++stage)
    {
      auto& coeffs = highCutCoeffs_[static_cast<size_t>(stage)];
      auto& state = highCutStates_[static_cast<size_t>(stage)];
      for (FrameCount i = 0; i < numFrames; ++i)
        output[i] = tick(coeffs, state, output[i]);
    }
  }
}

void GraphicEQ::reset()
{
  for (auto& s : states_)
  {
    s.z1 = 0.0f;
    s.z2 = 0.0f;
  }
  for (auto& s : lowCutStates_)
  {
    s.z1 = 0.0f;
    s.z2 = 0.0f;
  }
  for (auto& s : highCutStates_)
  {
    s.z1 = 0.0f;
    s.z2 = 0.0f;
  }
}

void GraphicEQ::updateCoefficients(int slot)
{
  auto idx = static_cast<size_t>(slot);
  float gainDb = gainsDb_[idx];

  if (!active_[idx] || std::fabs(gainDb) < 0.01f)
  {
    // Pass-through: unity gain biquad
    coeffs_[idx] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    activeNodeMask_ &= ~(1u << slot);
    return;
  }

  activeNodeMask_ |= (1u << slot);

  double q = static_cast<double>(computeQ(std::fabs(gainDb)));
  PeakingCoeffsD c = computePeakingCoeffs(static_cast<double>(gainDb),
                                          static_cast<double>(freqsHz_[idx]), q, sampleRate_);

  // Filter state (z1/z2) is intentionally left intact across coefficient
  // changes so live frequency/gain drags do not click or reset the filter.
  coeffs_[idx].b0 = static_cast<float>(c.b0);
  coeffs_[idx].b1 = static_cast<float>(c.b1);
  coeffs_[idx].b2 = static_cast<float>(c.b2);
  coeffs_[idx].a1 = static_cast<float>(c.a1);
  coeffs_[idx].a2 = static_cast<float>(c.a2);
}

void GraphicEQ::updateLowCutCoefficients()
{
  for (int stage = 0; stage < kNumCutStages; ++stage)
  {
    auto idx = static_cast<size_t>(stage);
    if (!lowCutActive_)
    {
      lowCutCoeffs_[idx] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
      continue;
    }

    PeakingCoeffsD c = computeHighpassCoeffs(static_cast<double>(lowCutFreqHz_),
                                             static_cast<double>(kCutStageQ[idx]), sampleRate_);
    lowCutCoeffs_[idx].b0 = static_cast<float>(c.b0);
    lowCutCoeffs_[idx].b1 = static_cast<float>(c.b1);
    lowCutCoeffs_[idx].b2 = static_cast<float>(c.b2);
    lowCutCoeffs_[idx].a1 = static_cast<float>(c.a1);
    lowCutCoeffs_[idx].a2 = static_cast<float>(c.a2);
  }
}

void GraphicEQ::updateHighCutCoefficients()
{
  for (int stage = 0; stage < kNumCutStages; ++stage)
  {
    auto idx = static_cast<size_t>(stage);
    if (!highCutActive_)
    {
      highCutCoeffs_[idx] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
      continue;
    }

    PeakingCoeffsD c = computeLowpassCoeffs(static_cast<double>(highCutFreqHz_),
                                            static_cast<double>(kCutStageQ[idx]), sampleRate_);
    highCutCoeffs_[idx].b0 = static_cast<float>(c.b0);
    highCutCoeffs_[idx].b1 = static_cast<float>(c.b1);
    highCutCoeffs_[idx].b2 = static_cast<float>(c.b2);
    highCutCoeffs_[idx].a1 = static_cast<float>(c.a1);
    highCutCoeffs_[idx].a2 = static_cast<float>(c.a2);
  }
}

Sample GraphicEQ::tick(const BiquadCoeffs& c, BiquadState& s, Sample input)
{
  // Transposed Direct Form II
  Sample output = c.b0 * input + s.z1;
  s.z1 = c.b1 * input - c.a1 * output + s.z2;
  s.z2 = c.b2 * input - c.a2 * output;
  return output;
}

float GraphicEQ::computeQ(float absGainDb)
{
  // Proportional Q: wider bandwidth at low gain, narrower at high gain.
  // Modeled after the API 550A characteristic.
  float normalized = absGainDb / MaxGraphicEQGainDb;
  normalized = std::max(0.0f, std::min(1.0f, normalized));
  return kQMin * std::pow(kQMax / kQMin, normalized);
}

float GraphicEQ::computeMagnitudeResponseDb(const bool* active, const float* freqsHz,
                                            const float* gainsDb, int numNodes, bool lowCutActive,
                                            float lowCutFreqHz, bool highCutActive,
                                            float highCutFreqHz, float freqHz,
                                            SampleRate sampleRate)
{
  // Use double precision and complex evaluation to avoid catastrophic
  // float cancellation at low frequencies where cos(w) ~ 1.
  const double pi = 3.14159265358979323846;
  double w = 2.0 * pi * static_cast<double>(freqHz) / sampleRate;
  double sinw = std::sin(w);
  double cosw = std::cos(w);
  double sin2w = std::sin(2.0 * w);
  double cos2w = std::cos(2.0 * w);

  double totalMagSq = 1.0;

  for (int i = 0; i < numNodes; ++i)
  {
    double gainDb = static_cast<double>(gainsDb[i]);
    if (!active[i] || std::fabs(gainDb) < 0.01)
      continue;

    double q = static_cast<double>(computeQ(static_cast<float>(std::fabs(gainDb))));
    PeakingCoeffsD c = computePeakingCoeffs(gainDb, static_cast<double>(freqsHz[i]), q, sampleRate);
    accumulateMagnitudeSq(c, cosw, sinw, cos2w, sin2w, totalMagSq);
  }

  for (int stage = 0; stage < kNumCutStages; ++stage)
  {
    auto idx = static_cast<size_t>(stage);
    if (lowCutActive)
    {
      PeakingCoeffsD c = computeHighpassCoeffs(static_cast<double>(lowCutFreqHz),
                                               static_cast<double>(kCutStageQ[idx]), sampleRate);
      accumulateMagnitudeSq(c, cosw, sinw, cos2w, sin2w, totalMagSq);
    }
    if (highCutActive)
    {
      PeakingCoeffsD c = computeLowpassCoeffs(static_cast<double>(highCutFreqHz),
                                              static_cast<double>(kCutStageQ[idx]), sampleRate);
      accumulateMagnitudeSq(c, cosw, sinw, cos2w, sin2w, totalMagSq);
    }
  }

  if (totalMagSq <= 0.0)
    return -200.0f;

  return static_cast<float>(10.0 * std::log10(totalMagSq));
}

}  // namespace octob
