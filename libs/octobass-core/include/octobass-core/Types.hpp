#pragma once

#include <cmath>
#include <octobir-core/Types.hpp>

namespace octob
{

// log2/exp2-based dB conversions: on ARM, log2f/exp2f map more directly to
// hardware than log10f/powf, avoiding the extra multiply inside the library.
constexpr float Log2ToDb = 6.02059991f;     // 20 * log10(2)
constexpr float DbToLog2 = 0.16609640474f;  // 1 / (20 * log10(2))

inline float dbToLinear(float db)
{
  return std::exp2(db * DbToLog2);
}

inline float linearToDb(float linear)
{
  if (linear < 1e-30f)
    return -96.0f;
  return std::log2(linear) * Log2ToDb;
}

// Crossover defaults and limits
constexpr float DefaultCrossoverFrequency = 250.0f;
constexpr float MinCrossoverFrequency = 50.0f;
constexpr float MaxCrossoverFrequency = 800.0f;

// Level defaults and limits
constexpr float DefaultBandLevelDb = 0.0f;
constexpr float MinBandLevelDb = -24.0f;
constexpr float MaxBandLevelDb = 12.0f;

constexpr float DefaultOutputGainDb = 0.0f;
constexpr float MinOutputGainDb = -24.0f;
constexpr float MaxOutputGainDb = 24.0f;

constexpr float DefaultDryWetMix = 1.0f;

// Compressor (squash) defaults and limits
constexpr float DefaultSquashAmount = 0.0f;
constexpr float MinSquashAmount = 0.0f;
constexpr float MaxSquashAmount = 1.0f;

constexpr int DefaultCompressionMode = 0;
constexpr int NumCompressionModes = 4;

// High band gain stages
constexpr float DefaultHighInputGainDb = 0.0f;
constexpr float MinHighInputGainDb = -24.0f;
constexpr float MaxHighInputGainDb = 24.0f;

constexpr float DefaultHighOutputGainDb = 0.0f;
constexpr float MinHighOutputGainDb = -24.0f;
constexpr float MaxHighOutputGainDb = 24.0f;

// High band wet/dry blend
constexpr float DefaultHighBandMix = 1.0f;

// NAM model quality/CPU trade-off (0.0 slimmest, 1.0 full quality)
constexpr float DefaultNamQuality = 1.0f;

// Noise gate defaults and limits
constexpr float DefaultGateThresholdDb = -96.0f;
constexpr float MinGateThresholdDb = -96.0f;
constexpr float MaxGateThresholdDb = 0.0f;

// Graphic EQ defaults and limits
constexpr int kGraphicEQNumNodes = 16;
constexpr float DefaultGraphicEQGainDb = 0.0f;
constexpr float MinGraphicEQGainDb = -12.0f;
constexpr float MaxGraphicEQGainDb = 12.0f;
constexpr float DefaultGraphicEQFreqHz = 1000.0f;
constexpr float MinGraphicEQFreqHz = 20.0f;
constexpr float MaxGraphicEQFreqHz = 20000.0f;

// Gain magnitude below which a node is treated as bypassed, shared between
// the DSP and any display so they agree on what counts as active
constexpr float GraphicEQBypassGainDb = 0.01f;

}  // namespace octob
