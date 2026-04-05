#include <gtest/gtest.h>

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

TEST_F(OctoBassProcessorTest, PassThrough)
{
  const double sampleRate = 44100.0;
  const int blockSize = 512;

  processor.prepareToPlay(sampleRate, blockSize);

  juce::AudioBuffer<float> buffer(2, blockSize);
  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < blockSize; ++i)
      buffer.setSample(ch, i,
                       std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f *
                                static_cast<float>(i) / static_cast<float>(sampleRate)));

  juce::AudioBuffer<float> expected(buffer);
  juce::MidiBuffer midi;

  processor.processBlock(buffer, midi);

  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < blockSize; ++i)
      EXPECT_FLOAT_EQ(buffer.getSample(ch, i), expected.getSample(ch, i))
          << "Mismatch at channel " << ch << " sample " << i;

  processor.releaseResources();
}

TEST_F(OctoBassProcessorTest, StateRoundTrip)
{
  juce::MemoryBlock stateData;
  processor.getStateInformation(stateData);
  EXPECT_GT(stateData.getSize(), 0u);

  processor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
}
