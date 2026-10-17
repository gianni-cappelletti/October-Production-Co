#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "octobass-core/BassProcessor.hpp"

using namespace octob;

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
constexpr size_t kBlockSize = 128;
constexpr size_t kBlocksBeforeChange = 40;
constexpr size_t kTotalBlocks = 64;
constexpr size_t kChangeSample = kBlocksBeforeChange * kBlockSize;
constexpr size_t kTotalSamples = kTotalBlocks * kBlockSize;
constexpr size_t kOnsetWindow = 8;
constexpr size_t kSettleSamples = static_cast<size_t>(kSampleRate * 0.010);

using Setter = void (BassProcessor::*)(float);

struct SmoothingCase
{
  std::string name;
  Setter setter;
  float from;
  float to;
  std::function<void(BassProcessor&)> configure = [](BassProcessor&) {};
};

void PrintTo(const SmoothingCase& smoothingCase, std::ostream* stream)
{
  *stream << smoothingCase.name;
}

std::vector<float> bassAndTrebleInput()
{
  std::vector<float> input(kTotalSamples);
  for (size_t i = 0; i < kTotalSamples; ++i)
  {
    const double t = static_cast<double>(i) / kSampleRate;
    input[i] = static_cast<float>(0.3 * std::sin(2.0 * kPi * 80.0 * t) +
                                  0.2 * std::sin(2.0 * kPi * 2000.0 * t));
  }
  return input;
}

void prepare(BassProcessor& processor)
{
  processor.setSampleRate(kSampleRate);
  processor.setMaxBlockSize(kBlockSize);
}

std::vector<float> render(const SmoothingCase& smoothingCase, float before, float after)
{
  BassProcessor processor;
  prepare(processor);
  smoothingCase.configure(processor);

  const auto input = bassAndTrebleInput();
  std::vector<float> output(kTotalSamples);
  for (size_t block = 0; block < kTotalBlocks; ++block)
  {
    (processor.*smoothingCase.setter)(block < kBlocksBeforeChange ? before : after);
    const size_t offset = block * kBlockSize;
    processor.processMono(input.data() + offset, output.data() + offset, kBlockSize);
  }
  return output;
}

float maxDifference(const std::vector<float>& a, const std::vector<float>& b, size_t start,
                    size_t length)
{
  float largest = 0.0f;
  for (size_t i = start; i < start + length; ++i)
    largest = std::max(largest, std::abs(a[i] - b[i]));
  return largest;
}

}  // namespace

class BassProcessorSmoothingTest : public ::testing::TestWithParam<SmoothingCase>
{
};

TEST_P(BassProcessorSmoothingTest, ChangeRampsInsteadOfStepping)
{
  const auto& smoothingCase = GetParam();
  const auto heldAtOldValue = render(smoothingCase, smoothingCase.from, smoothingCase.from);
  const auto heldAtNewValue = render(smoothingCase, smoothingCase.to, smoothingCase.to);
  const auto changed = render(smoothingCase, smoothingCase.from, smoothingCase.to);

  const float fullStep = maxDifference(heldAtOldValue, heldAtNewValue, kChangeSample, kOnsetWindow);
  ASSERT_GT(fullStep, 1.0e-3f) << "the change must be audible for this case to test anything";

  const float onsetStep = maxDifference(heldAtOldValue, changed, kChangeSample, kOnsetWindow);
  EXPECT_LT(onsetStep, 0.1f * fullStep);
}

TEST_P(BassProcessorSmoothingTest, ChangeSettlesOnNewValueWithinTenMilliseconds)
{
  const auto& smoothingCase = GetParam();
  const auto heldAtNewValue = render(smoothingCase, smoothingCase.to, smoothingCase.to);
  const auto changed = render(smoothingCase, smoothingCase.from, smoothingCase.to);

  const size_t settledStart = kChangeSample + kSettleSamples;
  EXPECT_LT(maxDifference(heldAtNewValue, changed, settledStart, kTotalSamples - settledStart),
            1.0e-6f);
}

TEST_P(BassProcessorSmoothingTest, OutputBeforeAnyChangeIsUnaffected)
{
  const auto& smoothingCase = GetParam();
  const auto heldAtOldValue = render(smoothingCase, smoothingCase.from, smoothingCase.from);
  const auto changed = render(smoothingCase, smoothingCase.from, smoothingCase.to);

  EXPECT_EQ(maxDifference(heldAtOldValue, changed, 0, kChangeSample), 0.0f);
}

INSTANTIATE_TEST_SUITE_P(
    EveryLevelAndMix, BassProcessorSmoothingTest,
    ::testing::Values(SmoothingCase{"LowBandLevel", &BassProcessor::setLowBandLevel, 0.0f, 12.0f},
                      SmoothingCase{"HighInputGain", &BassProcessor::setHighInputGain, 0.0f, 12.0f},
                      SmoothingCase{"HighOutputGain", &BassProcessor::setHighOutputGain, 0.0f,
                                    12.0f},
                      SmoothingCase{"OutputGain", &BassProcessor::setOutputGain, 0.0f, -12.0f},
                      SmoothingCase{"DryWet", &BassProcessor::setDryWetMix, 1.0f, 0.0f,
                                    [](BassProcessor& p) { p.setOutputGain(-12.0f); }},
                      SmoothingCase{"HighBandMixToDry", &BassProcessor::setHighBandMix, 1.0f, 0.0f,
                                    [](BassProcessor& p) { p.setHighOutputGain(-12.0f); }},
                      SmoothingCase{"HighBandMixToWet", &BassProcessor::setHighBandMix, 0.0f, 1.0f,
                                    [](BassProcessor& p) { p.setHighOutputGain(-12.0f); }}),
    [](const ::testing::TestParamInfo<SmoothingCase>& info) { return info.param.name; });

TEST(BassProcessorSmoothing, ValuesSetBeforeTheFirstBlockApplyFromTheFirstSample)
{
  constexpr float kOutputGainDb = -12.0f;
  const float outputGain = std::pow(10.0f, kOutputGainDb / 20.0f);
  const auto input = bassAndTrebleInput();

  BassProcessor unity;
  prepare(unity);
  BassProcessor attenuated;
  prepare(attenuated);
  attenuated.setOutputGain(kOutputGainDb);

  std::vector<float> unityOutput(kBlockSize);
  std::vector<float> attenuatedOutput(kBlockSize);
  unity.processMono(input.data(), unityOutput.data(), kBlockSize);
  attenuated.processMono(input.data(), attenuatedOutput.data(), kBlockSize);

  for (size_t i = 0; i < kBlockSize; ++i)
    EXPECT_NEAR(attenuatedOutput[i], unityOutput[i] * outputGain, 1.0e-6f) << "sample " << i;
}

TEST(BassProcessorSmoothing, ValuesChangedBeforeResetApplyFromTheFirstSampleAfterIt)
{
  const auto input = bassAndTrebleInput();

  BassProcessor fresh;
  prepare(fresh);
  fresh.setOutputGain(-12.0f);

  BassProcessor reused;
  prepare(reused);
  std::vector<float> discarded(kBlockSize);
  for (size_t block = 0; block < 4; ++block)
    reused.processMono(input.data() + block * kBlockSize, discarded.data(), kBlockSize);
  reused.setOutputGain(-12.0f);
  reused.reset();

  std::vector<float> freshOutput(kBlockSize);
  std::vector<float> reusedOutput(kBlockSize);
  fresh.processMono(input.data(), freshOutput.data(), kBlockSize);
  reused.processMono(input.data(), reusedOutput.data(), kBlockSize);

  for (size_t i = 0; i < kBlockSize; ++i)
    EXPECT_NEAR(reusedOutput[i], freshOutput[i], 1.0e-6f) << "sample " << i;
}
