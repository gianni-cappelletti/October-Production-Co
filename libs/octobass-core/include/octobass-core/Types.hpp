#pragma once

#include <octobir-core/Types.hpp>

namespace octob
{

// Crossover defaults and limits
constexpr float DefaultCrossoverFrequency = 250.0f;
constexpr float MinCrossoverFrequency = 50.0f;
constexpr float MaxCrossoverFrequency = 1000.0f;

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

}  // namespace octob
