#pragma once

#include <array>

// Constants describing the legacy 24 fixed-band graphic EQ, kept only so
// setStateInformation can migrate old saved state into the node-based EQ.
// These are a plugin-layer concern: the core library no longer knows about
// the fixed-band layout.
namespace legacyeq
{

constexpr int kNumBands = 24;

// Center frequencies of the legacy bands (sqrt(lowHz * highHz) of the
// SpectrumAnalyzer ranges)
constexpr std::array<float, kNumBands> kCenterFreqs = {{
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

}  // namespace legacyeq
