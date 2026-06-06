#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "Types.hpp"

namespace octob
{

struct GraphicEQNode
{
  bool active = false;
  float freqHz = DefaultGraphicEQFreqHz;
  float gainDb = DefaultGraphicEQGainDb;
};

struct GraphicEQCut
{
  bool active = false;
  float freqHz = DefaultGraphicEQFreqHz;
};

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
  static float computeMagnitudeResponseDb(const GraphicEQNode* nodes, int numNodes,
                                          const GraphicEQCut& lowCut, const GraphicEQCut& highCut,
                                          float freqHz, SampleRate sampleRate);

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
  void updateCutCoefficients(bool active, float freqHz, bool isHighpass,
                             std::array<BiquadCoeffs, kNumCutStages>& coeffs);

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
