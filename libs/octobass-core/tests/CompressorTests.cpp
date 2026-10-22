#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "octobass-core/Compressor.hpp"

using namespace octob;

namespace
{

constexpr int kBlockSize = 512;

std::vector<float> generateSine(float freqHz, float sampleRate, size_t numSamples,
                                float amplitude = 0.5f)
{
  std::vector<float> buf(numSamples);
  for (size_t i = 0; i < numSamples; ++i)
    buf[i] = amplitude * static_cast<float>(std::sin(2.0 * Pi * freqHz * i / sampleRate));
  return buf;
}

std::vector<float> processInBlocks(Compressor& comp, const std::vector<float>& input, int blockSize)
{
  std::vector<float> output(input.size());
  size_t pos = 0;
  while (pos < input.size())
  {
    size_t toProcess = std::min(static_cast<size_t>(blockSize), input.size() - pos);
    comp.process(input.data() + pos, output.data() + pos, toProcess);
    pos += toProcess;
  }
  return output;
}

}  // namespace

class CompressorTest : public ::testing::Test
{
 protected:
  void SetUp() override { comp.setSampleRate(44100.0); }

  Compressor comp;
};

TEST_F(CompressorTest, InitialDefaults)
{
  EXPECT_FLOAT_EQ(comp.getSquash(), DefaultSquashAmount);
  EXPECT_EQ(comp.getMode(), DefaultCompressionMode);
}

TEST_F(CompressorTest, SquashZero_NoChange)
{
  constexpr size_t kNumSamples = 4096;
  auto input = generateSine(100.0f, 44100.0f, kNumSamples, 0.5f);

  for (int mode = 0; mode < NumCompressionModes; ++mode)
  {
    comp.reset();
    comp.setMode(mode);
    comp.setSquash(0.0f);

    auto output = processInBlocks(comp, input, kBlockSize);

    float maxErr = 0.0f;
    for (size_t i = 0; i < kNumSamples; ++i)
      maxErr = std::max(maxErr, std::abs(output[i] - input[i]));

    EXPECT_LT(maxErr, 1e-5f) << "Mode " << mode << " should be transparent at squash=0";
  }
}

TEST_F(CompressorTest, SilentInput_SilentOutput)
{
  constexpr size_t kNumSamples = kBlockSize;
  std::vector<float> silence(kNumSamples, 0.0f);
  std::vector<float> output(kNumSamples, 1.0f);

  for (int mode = 0; mode < NumCompressionModes; ++mode)
  {
    comp.reset();
    comp.setMode(mode);
    comp.setSquash(1.0f);

    comp.process(silence.data(), output.data(), kNumSamples);

    for (size_t i = 0; i < kNumSamples; ++i)
      EXPECT_FLOAT_EQ(output[i], 0.0f) << "Mode " << mode << " sample " << i;
  }
}

TEST_F(CompressorTest, Reset_ClearsEnvelope)
{
  constexpr size_t kNumSamples = 4096;
  auto loud = generateSine(100.0f, 44100.0f, kNumSamples, 0.8f);
  std::vector<float> output(kNumSamples);

  comp.setSquash(1.0f);
  comp.process(loud.data(), output.data(), kNumSamples);

  comp.reset();

  std::vector<float> silence(kNumSamples, 0.0f);
  comp.process(silence.data(), output.data(), kNumSamples);

  for (size_t i = 0; i < kNumSamples; ++i)
    EXPECT_FLOAT_EQ(output[i], 0.0f) << "After reset, silence should produce silence at " << i;
}

TEST_F(CompressorTest, BlockBoundaryContinuity)
{
  constexpr size_t kTotalSamples = 8192;
  constexpr size_t kSmallBlock = 128;
  constexpr size_t kSkip = 2048;
  auto input = generateSine(100.0f, 44100.0f, kTotalSamples, 0.5f);

  // Process as one large block
  Compressor compA;
  compA.setSampleRate(44100.0);
  compA.setSquash(0.7f);
  std::vector<float> outputA(kTotalSamples);
  compA.process(input.data(), outputA.data(), kTotalSamples);

  // Process as many small blocks
  Compressor compB;
  compB.setSampleRate(44100.0);
  compB.setSquash(0.7f);
  std::vector<float> outputB(kTotalSamples);
  for (size_t b = 0; b < kTotalSamples / kSmallBlock; ++b)
    compB.process(input.data() + b * kSmallBlock, outputB.data() + b * kSmallBlock, kSmallBlock);

  float maxErr = 0.0f;
  for (size_t i = kSkip; i < kTotalSamples; ++i)
    maxErr = std::max(maxErr, std::abs(outputA[i] - outputB[i]));

  EXPECT_LT(maxErr, 1e-4f) << "Block boundary discontinuity: max error = " << maxErr;
}

TEST_F(CompressorTest, DifferentSampleRates)
{
  constexpr size_t kNumSamples = 4096;

  for (double sr : {44100.0, 48000.0, 96000.0})
  {
    for (int mode = 0; mode < NumCompressionModes; ++mode)
    {
      Compressor c;
      c.setSampleRate(sr);
      c.setMode(mode);
      c.setSquash(1.0f);

      auto input = generateSine(80.0f, static_cast<float>(sr), kNumSamples, 0.5f);
      auto output = processInBlocks(c, input, kBlockSize);

      float peak = 0.0f;
      for (float s : output)
        peak = std::max(peak, std::abs(s));

      EXPECT_GT(peak, 1e-6f) << "SR=" << sr << " mode=" << mode << " should produce output";
      EXPECT_FALSE(std::isnan(peak)) << "SR=" << sr << " mode=" << mode << " produced NaN";
    }
  }
}

// Regression: DAWs call setMode/setSquash every processBlock even when values
// haven't changed. setMode must not reset envelope state on redundant calls,
// otherwise gain discontinuities appear at every block boundary.
TEST_F(CompressorTest, RedundantSetMode_DoesNotResetState)
{
  constexpr size_t kBlockSize = 512;
  constexpr size_t kTotalSamples = 8192;
  constexpr size_t kSkip = 2048;
  auto input = generateSine(80.0f, 44100.0f, kTotalSamples, 0.5f);

  // Process without redundant setMode calls
  Compressor compA;
  compA.setSampleRate(44100.0);
  compA.setMode(0);
  compA.setSquash(1.0f);
  auto outputA = processInBlocks(compA, input, kBlockSize);

  // Process WITH redundant setMode calls every block (simulates DAW behavior)
  Compressor compB;
  compB.setSampleRate(44100.0);
  compB.setMode(0);
  compB.setSquash(1.0f);
  std::vector<float> outputB(kTotalSamples);
  for (size_t pos = 0; pos < kTotalSamples; pos += kBlockSize)
  {
    compB.setMode(0);
    compB.setSquash(1.0f);
    size_t n = std::min(kBlockSize, kTotalSamples - pos);
    compB.process(input.data() + pos, outputB.data() + pos, n);
  }

  float maxErr = 0.0f;
  for (size_t i = kSkip; i < kTotalSamples; ++i)
    maxErr = std::max(maxErr, std::abs(outputA[i] - outputB[i]));

  EXPECT_LT(maxErr, 1e-4f) << "Redundant setMode calls should not alter output (max error = "
                           << maxErr << ")";
}

TEST_F(CompressorTest, ModeSwitching_ResetsState)
{
  constexpr size_t kNumSamples = 4096;
  auto loud = generateSine(100.0f, 44100.0f, kNumSamples, 0.8f);
  std::vector<float> output(kNumSamples);

  comp.setSquash(1.0f);
  comp.process(loud.data(), output.data(), kNumSamples);

  comp.setMode(2);

  std::vector<float> silence(kNumSamples, 0.0f);
  comp.process(silence.data(), output.data(), kNumSamples);

  for (size_t i = 0; i < kNumSamples; ++i)
    EXPECT_FLOAT_EQ(output[i], 0.0f)
        << "After mode switch, silence should produce silence at " << i;
}
