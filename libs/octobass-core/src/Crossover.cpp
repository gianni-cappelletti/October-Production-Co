#include "octobass-core/Crossover.hpp"

#include <algorithm>
#include <cmath>

namespace octob
{

Crossover::Crossover() : frequency_(DefaultCrossoverFrequency), sampleRate_(44100.0)
{
  updateCoefficients();
}

void Crossover::setSampleRate(SampleRate sampleRate)
{
  if (sampleRate > 0.0)
  {
    sampleRate_ = sampleRate;
    updateCoefficients();
  }
}

void Crossover::setFrequency(float frequencyHz)
{
  frequency_ = std::max(MinCrossoverFrequency, std::min(MaxCrossoverFrequency, frequencyHz));
  updateCoefficients();
}

void Crossover::process(const Sample* input, Sample* lowOut, Sample* highOut, FrameCount numFrames)
{
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    Sample in = input[i];

    // LR4 low: two cascaded Butterworth LP SVF stages
    Sample lp1 = tickLP(coeffs_, lpState1_, in);
    Sample lp2 = tickLP(coeffs_, lpState2_, lp1);
    lowOut[i] = lp2;

    // LR4 high: two cascaded Butterworth HP SVF stages
    Sample hp1 = tickHP(coeffs_, hpState1_, in);
    Sample hp2 = tickHP(coeffs_, hpState2_, hp1);
    highOut[i] = hp2;
  }
}

void Crossover::reset()
{
  lpState1_ = SVFState();
  lpState2_ = SVFState();
  hpState1_ = SVFState();
  hpState2_ = SVFState();
}

void Crossover::updateCoefficients()
{
  // Cytomic SVF (Andy Simper, trapezoidal integration).
  // LR4 = two cascaded Butterworth 2nd-order SVFs per band.
  const double fc = static_cast<double>(frequency_);
  const double fs = sampleRate_;

  const double g = std::tan(Pi * fc / fs);
  const double k = std::sqrt(2.0);  // Butterworth: Q = 1/sqrt(2), k = 1/Q = sqrt(2)

  const double a1 = 1.0 / (1.0 + g * (g + k));
  const double a2 = g * a1;
  const double a3 = g * a2;

  coeffs_.g = static_cast<float>(g);
  coeffs_.k = static_cast<float>(k);
  coeffs_.a1 = static_cast<float>(a1);
  coeffs_.a2 = static_cast<float>(a2);
  coeffs_.a3 = static_cast<float>(a3);
}

Sample Crossover::tickLP(const SVFCoeffs& c, SVFState& s, Sample input)
{
  Sample v3 = input - s.ic2eq;
  Sample v1 = c.a1 * s.ic1eq + c.a2 * v3;
  Sample v2 = s.ic2eq + c.a2 * s.ic1eq + c.a3 * v3;
  s.ic1eq = 2.0f * v1 - s.ic1eq;
  s.ic2eq = 2.0f * v2 - s.ic2eq;
  return v2;
}

Sample Crossover::tickHP(const SVFCoeffs& c, SVFState& s, Sample input)
{
  Sample v3 = input - s.ic2eq;
  Sample v1 = c.a1 * s.ic1eq + c.a2 * v3;
  Sample v2 = s.ic2eq + c.a2 * s.ic1eq + c.a3 * v3;
  s.ic1eq = 2.0f * v1 - s.ic1eq;
  s.ic2eq = 2.0f * v2 - s.ic2eq;
  return input - c.k * v1 - v2;
}

}  // namespace octob
