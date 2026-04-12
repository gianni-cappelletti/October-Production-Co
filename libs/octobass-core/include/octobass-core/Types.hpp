#pragma once

#include <octobir-core/Types.hpp>

namespace octob
{

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

// Noise gate defaults and limits
constexpr float DefaultGateThresholdDb = -96.0f;
constexpr float MinGateThresholdDb = -96.0f;
constexpr float MaxGateThresholdDb = 0.0f;

// Graphic EQ defaults and limits
constexpr int kGraphicEQNumBands = 24;
constexpr float DefaultGraphicEQGainDb = 0.0f;
constexpr float MinGraphicEQGainDb = -12.0f;
constexpr float MaxGraphicEQGainDb = 12.0f;

}  // namespace octob
