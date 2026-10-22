#pragma once

#include <cstddef>

namespace octob
{

using SampleRate = double;
using Sample = float;
using FrameCount = size_t;

constexpr double Pi = 3.14159265358979323846;

constexpr int MaxIrLengthSeconds = 10;

constexpr float DbToLinearScalar = 0.1151292546497023f;  // ln(10) / 20
constexpr float IrCompensationGainDb = -18.0f;

}  // namespace octob
