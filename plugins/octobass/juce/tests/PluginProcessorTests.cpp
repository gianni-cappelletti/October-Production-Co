#include <gtest/gtest.h>

#include <cmath>

#include "PluginProcessor.h"

class OctoBassProcessorTest : public ::testing::Test
{
 protected:
  OctoBassProcessor processor;
};

TEST_F(OctoBassProcessorTest, CreateSuccessfully)
{
  EXPECT_EQ(processor.getName(), "OctoBASS");
}

TEST_F(OctoBassProcessorTest, HasEditor)
{
  EXPECT_TRUE(processor.hasEditor());
}

TEST_F(OctoBassProcessorTest, DoesNotAcceptMidi)
{
  EXPECT_FALSE(processor.acceptsMidi());
  EXPECT_FALSE(processor.producesMidi());
  EXPECT_FALSE(processor.isMidiEffect());
}

TEST_F(OctoBassProcessorTest, MagnitudePreserved)
{
  const double sampleRate = 44100.0;
  const int blockSize = 512;
  const int numBlocks = 16;
  const int totalSamples = numBlocks * blockSize;
  const int skipSamples = 4 * blockSize;

  processor.prepareToPlay(sampleRate, blockSize);

  // Generate a test tone
  juce::AudioBuffer<float> inputBuf(1, totalSamples);
  for (int i = 0; i < totalSamples; ++i)
    inputBuf.setSample(0, i,
                       0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 100.0f *
                                       static_cast<float>(i) / static_cast<float>(sampleRate)));

  // Process in blocks
  juce::AudioBuffer<float> buffer(1, blockSize);
  juce::MidiBuffer midi;
  std::vector<float> output(static_cast<size_t>(totalSamples));

  for (int b = 0; b < numBlocks; ++b)
  {
    for (int i = 0; i < blockSize; ++i)
      buffer.setSample(0, i, inputBuf.getSample(0, b * blockSize + i));

    processor.processBlock(buffer, midi);

    for (int i = 0; i < blockSize; ++i)
      output[static_cast<size_t>(b * blockSize + i)] = buffer.getSample(0, i);
  }

  // Compare RMS after warmup (crossover filter has transient)
  double inputRMS = 0.0, outputRMS = 0.0;
  for (int i = skipSamples; i < totalSamples; ++i)
  {
    float in = inputBuf.getSample(0, i);
    inputRMS += static_cast<double>(in) * in;
    outputRMS +=
        static_cast<double>(output[static_cast<size_t>(i)]) * output[static_cast<size_t>(i)];
  }
  int compareLen = totalSamples - skipSamples;
  inputRMS = std::sqrt(inputRMS / compareLen);
  outputRMS = std::sqrt(outputRMS / compareLen);

  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);
  EXPECT_NEAR(ratioDb, 0.0, 1.0) << "Default settings should preserve magnitude";

  processor.releaseResources();
}

TEST_F(OctoBassProcessorTest, SquashParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("squash");
  ASSERT_NE(param, nullptr);
  EXPECT_FLOAT_EQ(param->load(), 0.0f);
}

TEST_F(OctoBassProcessorTest, CompressionModeParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("compressionMode");
  ASSERT_NE(param, nullptr);
  EXPECT_FLOAT_EQ(param->load(), 0.0f);
}

TEST_F(OctoBassProcessorTest, CrossoverParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("crossoverFrequency");
  ASSERT_NE(param, nullptr);
  EXPECT_FLOAT_EQ(param->load(), 250.0f);
}

TEST_F(OctoBassProcessorTest, LowBandLevelParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("lowBandLevel");
  ASSERT_NE(param, nullptr);
  EXPECT_NEAR(param->load(), 0.0f, 0.01f);
}

TEST_F(OctoBassProcessorTest, HighInputGainParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("highInputGain");
  ASSERT_NE(param, nullptr);
  EXPECT_NEAR(param->load(), 0.0f, 0.01f);
}

TEST_F(OctoBassProcessorTest, HighOutputGainParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("highOutputGain");
  ASSERT_NE(param, nullptr);
  EXPECT_NEAR(param->load(), 0.0f, 0.01f);
}

TEST_F(OctoBassProcessorTest, NamNotLoadedByDefault)
{
  EXPECT_FALSE(processor.isNamModelLoaded());
}

TEST_F(OctoBassProcessorTest, StateRoundTrip)
{
  // Modify some parameters
  auto* squashParam = processor.getAPVTS().getParameter("squash");
  auto* modeParam = processor.getAPVTS().getParameter("compressionMode");
  ASSERT_NE(squashParam, nullptr);
  ASSERT_NE(modeParam, nullptr);

  squashParam->setValueNotifyingHost(0.7f);
  modeParam->setValueNotifyingHost(modeParam->convertTo0to1(2.0f));  // Punch mode

  // Save state
  juce::MemoryBlock stateData;
  processor.getStateInformation(stateData);
  EXPECT_GT(stateData.getSize(), 0u);

  // Restore into a new processor
  OctoBassProcessor processor2;
  processor2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

  auto* squash2 = processor2.getAPVTS().getRawParameterValue("squash");
  auto* mode2 = processor2.getAPVTS().getRawParameterValue("compressionMode");
  ASSERT_NE(squash2, nullptr);
  ASSERT_NE(mode2, nullptr);

  EXPECT_NEAR(squash2->load(), 0.7f, 0.02f);
  EXPECT_NEAR(mode2->load(), 2.0f, 0.1f);
}
