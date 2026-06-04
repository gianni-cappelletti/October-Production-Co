#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "Types.hpp"

namespace octob
{

class GraphicEQ
{
 public:
  GraphicEQ();

  void setSampleRate(SampleRate sampleRate);

  void setNodeActive(int slot, bool active);
  bool getNodeActive(int slot) const;

  void setNodeFrequency(int slot, float freqHz);
  float getNodeFrequency(int slot) const;

  void setNodeGain(int slot, float gainDb);
  float getNodeGain(int slot) const;

  void setNode(int slot, bool active, float freqHz, float gainDb);

  // Low/high cut filters: fixed 24 dB/octave (4th-order Butterworth)
  void setLowCut(bool active, float freqHz);
  bool getLowCutActive() const;
  float getLowCutFrequency() const;

  void setHighCut(bool active, float freqHz);
  bool getHighCutActive() const;
  float getHighCutFrequency() const;

  void process(const Sample* input, Sample* output, FrameCount numFrames);

  void reset();

  // Compute the combined magnitude response in dB at a given frequency.
  // This evaluates the actual biquad transfer function for all active nodes
  // and the low/high cut filters.
  static float computeMagnitudeResponseDb(const bool* active, const float* freqsHz,
                                          const float* gainsDb, int numNodes, bool lowCutActive,
                                          float lowCutFreqHz, bool highCutActive,
                                          float highCutFreqHz, float freqHz, SampleRate sampleRate);

  // Center frequencies of the legacy 24 fixed bands (sqrt(lowHz * highHz) of the
  // SpectrumAnalyzer ranges), retained for migrating old saved state into nodes.
  static constexpr std::array<float, kGraphicEQNumBands> kLegacyCenterFreqs = {{
      28.23f,    // Band 0:  <50 Hz combined
      50.10f,    // Band 1:  50 Hz
      63.13f,    // Band 2:  63 Hz
      79.69f,    // Band 3:  80 Hz
      100.00f,   // Band 4:  100 Hz
      125.74f,   // Band 5:  125 Hz
      159.05f,   // Band 6:  160 Hz
      199.96f,   // Band 7:  200 Hz
      250.49f,   // Band 8:  250 Hz
      315.63f,   // Band 9:  315 Hz
      398.43f,   // Band 10: 400 Hz
      500.95f,   // Band 11: 500 Hz
      631.26f,   // Band 12: 630 Hz
      796.90f,   // Band 13: 800 Hz
      1000.00f,  // Band 14: 1 kHz
      1257.43f,  // Band 15: 1.25 kHz
      1590.37f,  // Band 16: 1.6 kHz
      2000.00f,  // Band 17: 2 kHz
      2504.97f,  // Band 18: 2.5 kHz
      3156.26f,  // Band 19: 3.15 kHz
      3984.33f,  // Band 20: 4 kHz
      5009.88f,  // Band 21: 5 kHz
      6312.51f,  // Band 22: 6.3 kHz
      12624.69f  // Band 23: >6.3 kHz combined
  }};

  // Proportional-Q constants (API 550A-style)
  static constexpr float kQMin = 0.8f;
  static constexpr float kQMax = 8.0f;

  // Stage Q values for a 4th-order Butterworth cascade: 1/(2*cos(pi/8)), 1/(2*cos(3*pi/8))
  static constexpr int kNumCutStages = 2;
  static constexpr std::array<float, kNumCutStages> kCutStageQ = {{0.54119610f, 1.30656296f}};

  static float computeQ(float absGainDb);

 private:
  struct BiquadCoeffs
  {
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
  };

  struct BiquadState
  {
    float z1 = 0.0f;
    float z2 = 0.0f;
  };

  static bool isValidSlot(int slot);

  void updateCoefficients(int slot);
  void updateLowCutCoefficients();
  void updateHighCutCoefficients();

  static Sample tick(const BiquadCoeffs& c, BiquadState& s, Sample input);

  std::array<bool, kGraphicEQNumNodes> active_{};
  std::array<float, kGraphicEQNumNodes> freqsHz_{};
  std::array<float, kGraphicEQNumNodes> gainsDb_{};
  std::array<BiquadCoeffs, kGraphicEQNumNodes> coeffs_{};
  std::array<BiquadState, kGraphicEQNumNodes> states_{};

  bool lowCutActive_ = false;
  float lowCutFreqHz_ = MinGraphicEQFreqHz;
  std::array<BiquadCoeffs, kNumCutStages> lowCutCoeffs_{};
  std::array<BiquadState, kNumCutStages> lowCutStates_{};

  bool highCutActive_ = false;
  float highCutFreqHz_ = MaxGraphicEQFreqHz;
  std::array<BiquadCoeffs, kNumCutStages> highCutCoeffs_{};
  std::array<BiquadState, kNumCutStages> highCutStates_{};

  uint32_t activeNodeMask_ = 0;
  SampleRate sampleRate_ = 44100.0;
};

}  // namespace octob
