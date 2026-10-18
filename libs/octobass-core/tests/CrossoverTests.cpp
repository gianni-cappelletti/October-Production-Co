#include <gtest/gtest.h>

#include <cmath>
#include <numeric>
#include <vector>

#include "octobass-core/Crossover.hpp"

using namespace octob;

namespace
{

std::vector<float> generateSine(float freqHz, float sampleRate, size_t numSamples,
                                float amplitude = 1.0f)
{
  std::vector<float> buf(numSamples);
  for (size_t i = 0; i < numSamples; ++i)
    buf[i] = amplitude * static_cast<float>(std::sin(2.0 * Pi * freqHz * i / sampleRate));
  return buf;
}

std::vector<float> generateWhiteNoise(size_t numSamples, unsigned int seed = 42)
{
  std::vector<float> buf(numSamples);
  unsigned int state = seed;
  for (size_t i = 0; i < numSamples; ++i)
  {
    state = state * 1664525u + 1013904223u;
    buf[i] = static_cast<float>(static_cast<int>(state)) / static_cast<float>(0x7FFFFFFF);
  }
  return buf;
}

double computeRMS(const std::vector<float>& buf, size_t start = 0, size_t len = 0)
{
  if (len == 0)
    len = buf.size() - start;
  double sum = 0.0;
  for (size_t i = start; i < start + len; ++i)
    sum += static_cast<double>(buf[i]) * buf[i];
  return std::sqrt(sum / static_cast<double>(len));
}

// Verify magnitude-flat transfer function by measuring gain at discrete sine frequencies.
// Processes each sine through the crossover, sums LP+HP, and checks output RMS vs input RMS.
void assertFlatMagnitudeAtFrequencies(Crossover& xover, float sampleRate, const float* testFreqs,
                                      size_t numFreqs, double tolDb, const char* label)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  for (size_t f = 0; f < numFreqs; ++f)
  {
    xover.reset();
    auto input = generateSine(testFreqs[f], sampleRate, kNumSamples);

    std::vector<float> low(kNumSamples);
    std::vector<float> high(kNumSamples);
    xover.process(input.data(), low.data(), high.data(), kNumSamples);

    std::vector<float> sum(kNumSamples);
    for (size_t i = 0; i < kNumSamples; ++i)
      sum[i] = low[i] + high[i];

    double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
    double outputRMS = computeRMS(sum, kSkip, kNumSamples - kSkip);
    double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

    EXPECT_NEAR(ratioDb, 0.0, tolDb)
        << label << ": magnitude at " << testFreqs[f] << " Hz deviates by " << ratioDb << " dB";
  }
}

}  // namespace

class CrossoverTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    xover.setSampleRate(44100.0);
    xover.setFrequency(200.0f);
  }

  Crossover xover;
};

// LR4 produces an all-pass sum: |LP + HP| = 1 at all frequencies.
// The sum low+high equals the input delayed by the filter's group delay,
// so we verify that the RMS of the sum matches the input RMS (magnitude-flat).
TEST_F(CrossoverTest, AllPassReconstruction)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;
  auto input = generateWhiteNoise(kNumSamples);

  std::vector<float> low(kNumSamples);
  std::vector<float> high(kNumSamples);
  xover.process(input.data(), low.data(), high.data(), kNumSamples);

  // Sum the bands
  std::vector<float> sum(kNumSamples);
  for (size_t i = 0; i < kNumSamples; ++i)
    sum[i] = low[i] + high[i];

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double sumRMS = computeRMS(sum, kSkip, kNumSamples - kSkip);

  // RMS should match within 0.5 dB (all-pass magnitude-flat)
  double ratioDb = 20.0 * std::log10(sumRMS / inputRMS);
  EXPECT_NEAR(ratioDb, 0.0, 0.5) << "Sum RMS deviates from input by " << ratioDb
                                 << " dB (expected flat)";
}

// Verify reconstruction holds across multiple crossover frequencies
TEST_F(CrossoverTest, AllPassReconstruction_MultipleFrequencies)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;
  float frequencies[] = {60.0f, 100.0f, 200.0f, 400.0f};

  auto input = generateWhiteNoise(kNumSamples);

  for (float freq : frequencies)
  {
    Crossover xo;
    xo.setSampleRate(44100.0);
    xo.setFrequency(freq);

    std::vector<float> low(kNumSamples);
    std::vector<float> high(kNumSamples);
    xo.process(input.data(), low.data(), high.data(), kNumSamples);

    std::vector<float> sum(kNumSamples);
    for (size_t i = 0; i < kNumSamples; ++i)
      sum[i] = low[i] + high[i];

    double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
    double sumRMS = computeRMS(sum, kSkip, kNumSamples - kSkip);
    double ratioDb = 20.0 * std::log10(sumRMS / inputRMS);

    EXPECT_NEAR(ratioDb, 0.0, 0.5) << "Reconstruction failed at crossover freq = " << freq << " Hz";
  }
}

TEST_F(CrossoverTest, LowBandPassesBelow)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 2048;
  auto input = generateSine(60.0f, 44100.0f, kNumSamples);

  std::vector<float> low(kNumSamples);
  std::vector<float> high(kNumSamples);
  xover.process(input.data(), low.data(), high.data(), kNumSamples);

  double lowRMS = computeRMS(low, kSkip, kNumSamples - kSkip);
  double highRMS = computeRMS(high, kSkip, kNumSamples - kSkip);
  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);

  EXPECT_GT(lowRMS / inputRMS, 0.95) << "Low band should pass 60 Hz signal (crossover at 200 Hz)";
  EXPECT_LT(highRMS / inputRMS, 0.05) << "High band should reject 60 Hz signal";
}

TEST_F(CrossoverTest, HighBandPassesAbove)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 2048;
  auto input = generateSine(2000.0f, 44100.0f, kNumSamples);

  std::vector<float> low(kNumSamples);
  std::vector<float> high(kNumSamples);
  xover.process(input.data(), low.data(), high.data(), kNumSamples);

  double lowRMS = computeRMS(low, kSkip, kNumSamples - kSkip);
  double highRMS = computeRMS(high, kSkip, kNumSamples - kSkip);
  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);

  EXPECT_LT(lowRMS / inputRMS, 0.05) << "Low band should reject 2000 Hz signal";
  EXPECT_GT(highRMS / inputRMS, 0.95)
      << "High band should pass 2000 Hz signal (crossover at 200 Hz)";
}

TEST_F(CrossoverTest, Reset_ClearsState)
{
  constexpr size_t kNumSamples = 512;
  auto input = generateSine(100.0f, 44100.0f, kNumSamples);

  std::vector<float> low(kNumSamples);
  std::vector<float> high(kNumSamples);
  xover.process(input.data(), low.data(), high.data(), kNumSamples);

  xover.reset();

  std::vector<float> silence(kNumSamples, 0.0f);
  xover.process(silence.data(), low.data(), high.data(), kNumSamples);

  for (size_t i = 0; i < kNumSamples; ++i)
  {
    EXPECT_FLOAT_EQ(low[i], 0.0f) << "Low band not zero at sample " << i << " after reset";
    EXPECT_FLOAT_EQ(high[i], 0.0f) << "High band not zero at sample " << i << " after reset";
  }
}

TEST_F(CrossoverTest, FrequencyClamping)
{
  xover.setFrequency(10.0f);
  EXPECT_FLOAT_EQ(xover.getFrequency(), MinCrossoverFrequency);

  xover.setFrequency(5000.0f);
  EXPECT_FLOAT_EQ(xover.getFrequency(), MaxCrossoverFrequency);

  xover.setFrequency(300.0f);
  EXPECT_FLOAT_EQ(xover.getFrequency(), 300.0f);
}

TEST_F(CrossoverTest, DifferentSampleRates)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;
  double sampleRates[] = {44100.0, 48000.0, 96000.0};

  for (double sr : sampleRates)
  {
    Crossover xo;
    xo.setSampleRate(sr);
    xo.setFrequency(200.0f);

    auto input = generateWhiteNoise(kNumSamples);
    std::vector<float> low(kNumSamples);
    std::vector<float> high(kNumSamples);
    xo.process(input.data(), low.data(), high.data(), kNumSamples);

    std::vector<float> sum(kNumSamples);
    for (size_t i = 0; i < kNumSamples; ++i)
      sum[i] = low[i] + high[i];

    double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
    double sumRMS = computeRMS(sum, kSkip, kNumSamples - kSkip);
    double ratioDb = 20.0 * std::log10(sumRMS / inputRMS);

    EXPECT_NEAR(ratioDb, 0.0, 0.5) << "Reconstruction failed at sample rate = " << sr;
  }
}

// LR4 all-pass produces magnitude-flat reconstruction (LP+HP sums to unity gain at all
// frequencies) but introduces frequency-dependent group delay. We verify both broadband RMS
// preservation (white noise) and per-frequency magnitude flatness (sine sweep).
TEST_F(CrossoverTest, MagnitudeFlatReconstruction)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;
  auto input = generateWhiteNoise(kNumSamples);

  std::vector<float> low(kNumSamples);
  std::vector<float> high(kNumSamples);
  xover.process(input.data(), low.data(), high.data(), kNumSamples);

  std::vector<float> sum(kNumSamples);
  for (size_t i = 0; i < kNumSamples; ++i)
    sum[i] = low[i] + high[i];

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double sumRMS = computeRMS(sum, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(sumRMS / inputRMS);
  EXPECT_NEAR(ratioDb, 0.0, 0.1) << "LP+HP magnitude should match input within 0.1 dB";

  float testFreqs[] = {30.0f,  60.0f,  100.0f,  150.0f,  200.0f,
                       300.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f};
  assertFlatMagnitudeAtFrequencies(xover, 44100.0f, testFreqs, 10, 0.1, "LP+HP reconstruction");
}
