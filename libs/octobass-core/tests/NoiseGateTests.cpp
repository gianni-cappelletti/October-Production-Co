#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "octobass-core/NoiseGate.hpp"

using namespace octob;

namespace
{

constexpr int kSampleRate = 48000;
constexpr int kBlockSize = 512;

std::vector<float> generateDC(size_t numSamples, float amplitude)
{
  return std::vector<float>(numSamples, amplitude);
}

float peakLevel(const std::vector<float>& buf)
{
  float peak = 0.0f;
  for (float s : buf)
    peak = std::max(peak, std::abs(s));
  return peak;
}

}  // namespace

TEST(NoiseGateTest, DisabledByDefault)
{
  NoiseGate gate;
  gate.setSampleRate(kSampleRate);

  EXPECT_FALSE(gate.isEnabled());
  EXPECT_FLOAT_EQ(gate.getThresholdDb(), -96.0f);
}

TEST(NoiseGateTest, EnabledWhenThresholdAboveDisablePoint)
{
  NoiseGate gate;
  gate.setSampleRate(kSampleRate);

  gate.setThresholdDb(-60.0f);
  EXPECT_TRUE(gate.isEnabled());

  gate.setThresholdDb(-89.0f);
  EXPECT_TRUE(gate.isEnabled());

  gate.setThresholdDb(-91.0f);
  EXPECT_FALSE(gate.isEnabled());
}

TEST(NoiseGateTest, BypassWhenDisabled)
{
  NoiseGate gate;
  gate.setSampleRate(kSampleRate);

  auto key = generateDC(kBlockSize, 0.0f);
  auto signal = generateDC(kBlockSize, 0.5f);
  std::vector<float> output(kBlockSize);

  gate.process(key.data(), signal.data(), output.data(), kBlockSize);

  for (int i = 0; i < kBlockSize; ++i)
    EXPECT_FLOAT_EQ(output[i], 0.5f);
}

TEST(NoiseGateTest, PassesSignalWhenKeyAboveThreshold)
{
  NoiseGate gate;
  gate.setSampleRate(kSampleRate);
  gate.setThresholdDb(-40.0f);

  float keyAmplitude = dbToLinear(-20.0f);
  auto key = generateDC(kBlockSize, keyAmplitude);
  auto signal = generateDC(kBlockSize, 1.0f);
  std::vector<float> output(kBlockSize);

  // Prime the gate with several blocks so it fully opens
  for (int b = 0; b < 4; ++b)
    gate.process(key.data(), signal.data(), output.data(), kBlockSize);

  // Output should be close to full signal (gate open)
  float outPeak = peakLevel(output);
  EXPECT_GT(outPeak, 0.95f);
}

TEST(NoiseGateTest, AttenuatesSignalWhenKeyBelowThreshold)
{
  NoiseGate gate;
  gate.setSampleRate(kSampleRate);
  gate.setThresholdDb(-20.0f);
  gate.reset();

  float keyAmplitude = dbToLinear(-60.0f);
  auto key = generateDC(kBlockSize, keyAmplitude);
  auto signal = generateDC(kBlockSize, 1.0f);
  std::vector<float> output(kBlockSize);

  // Process enough blocks for the gate to fully close
  for (int b = 0; b < 20; ++b)
    gate.process(key.data(), signal.data(), output.data(), kBlockSize);

  float outPeak = peakLevel(output);
  // Should be heavily attenuated (range is -80 dB)
  EXPECT_LT(outPeak, 0.01f);
}

TEST(NoiseGateTest, OpensInstantlyOnTransient)
{
  NoiseGate gate;
  gate.setSampleRate(kSampleRate);
  gate.setThresholdDb(-30.0f);
  gate.reset();

  // Start with silence to ensure gate is closed
  auto silentKey = generateDC(kBlockSize, 0.0f);
  auto signal = generateDC(kBlockSize, 1.0f);
  std::vector<float> output(kBlockSize);

  for (int b = 0; b < 10; ++b)
    gate.process(silentKey.data(), signal.data(), output.data(), kBlockSize);

  // Verify gate is closed
  EXPECT_LT(peakLevel(output), 0.01f);

  // Now hit it with a loud transient
  float transientLevel = dbToLinear(-10.0f);
  auto loudKey = generateDC(kBlockSize, transientLevel);
  gate.process(loudKey.data(), signal.data(), output.data(), kBlockSize);

  // The gate should open very quickly -- check a sample near the start of the block
  // (not sample 0 since there's a ~0.01ms attack, but by sample 10 it should be mostly open)
  EXPECT_GT(std::abs(output[10]), 0.5f);
}

TEST(NoiseGateTest, HysteresisPreventsChattering)
{
  NoiseGate gate;
  gate.setSampleRate(kSampleRate);
  gate.setThresholdDb(-30.0f);

  // Key signal right between threshold and threshold-hysteresis
  // Threshold = -30 dB, close threshold = -36 dB
  // Signal at -33 dB is in the hysteresis zone
  float keyAmplitude = dbToLinear(-33.0f);
  auto key = generateDC(kBlockSize, keyAmplitude);
  auto signal = generateDC(kBlockSize, 1.0f);
  std::vector<float> output(kBlockSize);

  // First, open the gate with a loud signal
  float loudLevel = dbToLinear(-10.0f);
  auto loudKey = generateDC(kBlockSize, loudLevel);
  for (int b = 0; b < 4; ++b)
    gate.process(loudKey.data(), signal.data(), output.data(), kBlockSize);

  float openLevel = peakLevel(output);
  EXPECT_GT(openLevel, 0.9f);

  // Now switch to hysteresis-zone key -- gate should remain open (not close)
  for (int b = 0; b < 4; ++b)
    gate.process(key.data(), signal.data(), output.data(), kBlockSize);

  // Should still be passing signal since we haven't crossed the close threshold
  float hystLevel = peakLevel(output);
  EXPECT_GT(hystLevel, 0.5f);
}

TEST(NoiseGateTest, ThresholdClampedToRange)
{
  NoiseGate gate;

  gate.setThresholdDb(10.0f);
  EXPECT_LE(gate.getThresholdDb(), 0.0f);

  gate.setThresholdDb(-200.0f);
  EXPECT_GE(gate.getThresholdDb(), -96.0f);
}

TEST(NoiseGateTest, ResetClearsState)
{
  NoiseGate gate;
  gate.setSampleRate(kSampleRate);
  gate.setThresholdDb(-30.0f);

  // Open the gate
  float loudLevel = dbToLinear(-10.0f);
  auto key = generateDC(kBlockSize, loudLevel);
  auto signal = generateDC(kBlockSize, 1.0f);
  std::vector<float> output(kBlockSize);

  for (int b = 0; b < 4; ++b)
    gate.process(key.data(), signal.data(), output.data(), kBlockSize);

  EXPECT_GT(gate.getGateGain(), 0.9f);

  gate.reset();
  EXPECT_FLOAT_EQ(gate.getGateGain(), 0.0f);
}

TEST(NoiseGateTest, InPlaceProcessing)
{
  NoiseGate gate;
  gate.setSampleRate(kSampleRate);
  gate.setThresholdDb(-40.0f);

  float keyAmplitude = dbToLinear(-10.0f);
  auto key = generateDC(kBlockSize, keyAmplitude);
  std::vector<float> buffer(kBlockSize, 0.8f);

  // Process in-place (signalInput == signalOutput)
  for (int b = 0; b < 4; ++b)
    gate.process(key.data(), buffer.data(), buffer.data(), kBlockSize);

  EXPECT_GT(peakLevel(buffer), 0.7f);
}
