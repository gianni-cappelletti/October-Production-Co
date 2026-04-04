#pragma once

#include <cstddef>

namespace octob
{

using SampleRate = double;
using Sample = float;
using FrameCount = size_t;

constexpr int MaxIrLengthSeconds = 10;
constexpr int MinSampleRate = 8000;
constexpr int MaxSampleRate = 192000;

constexpr float DbToLinearScalar = 0.1151292546497023f;  // ln(10) / 20
constexpr float IrCompensationGainDb = -18.0f;

}  // namespace octob
