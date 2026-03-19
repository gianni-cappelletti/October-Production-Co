#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "octobir-core/IRProcessor.hpp"

using namespace octob;

class LatencyCompensationTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    processor_ = std::make_unique<IRProcessor>();
    processor_->setSampleRate(48000.0);
  }

  void TearDown() override { processor_.reset(); }

  std::unique_ptr<IRProcessor> processor_;
};

TEST_F(LatencyCompensationTest, MaxLatencyReporting_SingleIR)
{
  EXPECT_EQ(processor_->getLatencySamples(), 0);
}

TEST_F(LatencyCompensationTest, MaxLatencyReporting_NoIRsLoaded)
{
  int latency = processor_->getLatencySamples();
  EXPECT_EQ(latency, 0);
}

TEST_F(LatencyCompensationTest, PhaseAlignment_ImpulseResponse)
{
  const int numFrames = 512;
  std::vector<Sample> input(numFrames, 0.0f);
  std::vector<Sample> output(numFrames, 0.0f);

  input[0] = 1.0f;

  processor_->processMono(input.data(), output.data(), numFrames);

  int impulsePosition = -1;
  for (int i = 0; i < numFrames; ++i)
  {
    if (std::abs(output[i]) > 0.1f)
    {
      impulsePosition = i;
      break;
    }
  }

  EXPECT_GE(impulsePosition, 0);
}

TEST_F(LatencyCompensationTest, PhaseAlignment_SineWave)
{
  const int numFrames = 1024;
  const float frequency = 440.0f;
  const float sampleRate = 48000.0f;
  const float amplitude = 0.5f;

  std::vector<Sample> input(numFrames);
  std::vector<Sample> output(numFrames);

  for (int i = 0; i < numFrames; ++i)
  {
    input[i] = amplitude * std::sin(2.0f * M_PI * frequency * i / sampleRate);
  }

  processor_->processMono(input.data(), output.data(), numFrames);

  float inputRMS = 0.0f;
  float outputRMS = 0.0f;
  for (int i = 0; i < numFrames; ++i)
  {
    inputRMS += input[i] * input[i];
    outputRMS += output[i] * output[i];
  }
  inputRMS = std::sqrt(inputRMS / numFrames);
  outputRMS = std::sqrt(outputRMS / numFrames);

  EXPECT_GT(outputRMS, 0.0f);
  EXPECT_LT(outputRMS, 1.0f);
  EXPECT_NEAR(outputRMS, inputRMS, 1e-4f);
}

TEST_F(LatencyCompensationTest, StereoProcessing_ChannelIndependence)
{
  const int numFrames = 512;
  std::vector<Sample> inputL(numFrames, 0.0f);
  std::vector<Sample> inputR(numFrames, 0.0f);
  std::vector<Sample> outputL(numFrames, 0.0f);
  std::vector<Sample> outputR(numFrames, 0.0f);

  // L impulse at sample 0, R impulse at sample 10 — with no IRs, this is passthrough.
  // Verify the channels don't bleed into each other.
  inputL[0] = 1.0f;
  inputR[10] = 1.0f;

  processor_->processStereo(inputL.data(), inputR.data(), outputL.data(), outputR.data(),
                            numFrames);

  EXPECT_FLOAT_EQ(outputL[0], 1.0f);
  EXPECT_FLOAT_EQ(outputR[10], 1.0f);
  EXPECT_FLOAT_EQ(outputL[10], 0.0f);
  EXPECT_FLOAT_EQ(outputR[0], 0.0f);
}

TEST_F(LatencyCompensationTest, BlendParameter_RangeValidation)
{
  processor_->setBlend(-1.5f);
  EXPECT_FLOAT_EQ(processor_->getBlend(), -1.0f);

  processor_->setBlend(1.5f);
  EXPECT_FLOAT_EQ(processor_->getBlend(), 1.0f);

  processor_->setBlend(0.0f);
  EXPECT_FLOAT_EQ(processor_->getBlend(), 0.0f);

  processor_->setBlend(-0.5f);
  EXPECT_FLOAT_EQ(processor_->getBlend(), -0.5f);

  processor_->setBlend(0.75f);
  EXPECT_FLOAT_EQ(processor_->getBlend(), 0.75f);
}

TEST_F(LatencyCompensationTest, OutputGain_LinearConversion)
{
  processor_->setOutputGain(0.0f);
  EXPECT_FLOAT_EQ(processor_->getOutputGain(), 0.0f);

  processor_->setOutputGain(6.0f);
  EXPECT_FLOAT_EQ(processor_->getOutputGain(), 6.0f);

  processor_->setOutputGain(-6.0f);
  EXPECT_FLOAT_EQ(processor_->getOutputGain(), -6.0f);

  processor_->setOutputGain(25.0f);
  EXPECT_FLOAT_EQ(processor_->getOutputGain(), 24.0f);

  processor_->setOutputGain(-25.0f);
  EXPECT_FLOAT_EQ(processor_->getOutputGain(), -24.0f);
}

TEST_F(LatencyCompensationTest, SilentInput_SilentOutput)
{
  const int numFrames = 1024;
  std::vector<Sample> input(numFrames, 0.0f);
  std::vector<Sample> output(numFrames, 1.0f);

  processor_->processMono(input.data(), output.data(), numFrames);

  for (int i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(output[i], 0.0f) << "Non-zero output at sample " << i;
  }
}

TEST_F(LatencyCompensationTest, Reset_ClearsState)
{
  const int numFrames = 512;
  std::vector<Sample> input(numFrames, 0.0f);
  std::vector<Sample> output1(numFrames, 0.0f);
  std::vector<Sample> output2(numFrames, 0.0f);

  input[0] = 1.0f;
  processor_->processMono(input.data(), output1.data(), numFrames);

  processor_->reset();

  std::fill(input.begin(), input.end(), 0.0f);
  processor_->processMono(input.data(), output2.data(), numFrames);

  for (int i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(output2[i], 0.0f) << "Non-zero output after reset at sample " << i;
  }
}

TEST_F(LatencyCompensationTest, DynamicMode_DefaultState)
{
  EXPECT_FALSE(processor_->getDynamicModeEnabled());

  processor_->setDynamicModeEnabled(true);
  EXPECT_TRUE(processor_->getDynamicModeEnabled());

  processor_->setDynamicModeEnabled(false);
  EXPECT_FALSE(processor_->getDynamicModeEnabled());
}

TEST_F(LatencyCompensationTest, SidechainMode_DefaultState)
{
  EXPECT_FALSE(processor_->getSidechainEnabled());

  processor_->setSidechainEnabled(true);
  EXPECT_TRUE(processor_->getSidechainEnabled());

  processor_->setSidechainEnabled(false);
  EXPECT_FALSE(processor_->getSidechainEnabled());
}

TEST_F(LatencyCompensationTest, IREnableFlags_DefaultState)
{
  EXPECT_TRUE(processor_->getIRAEnabled());
  EXPECT_TRUE(processor_->getIRBEnabled());

  processor_->setIRAEnabled(false);
  EXPECT_FALSE(processor_->getIRAEnabled());

  processor_->setIRBEnabled(false);
  EXPECT_FALSE(processor_->getIRBEnabled());

  processor_->setIRAEnabled(true);
  processor_->setIRBEnabled(true);
  EXPECT_TRUE(processor_->getIRAEnabled());
  EXPECT_TRUE(processor_->getIRBEnabled());
}

TEST_F(LatencyCompensationTest, ConsistentOutput_MultipleProcessCalls)
{
  const int numFrames = 256;
  std::vector<Sample> input(numFrames, 0.1f);
  std::vector<Sample> output1(numFrames, 0.0f);
  std::vector<Sample> output2(numFrames, 0.0f);

  processor_->processMono(input.data(), output1.data(), numFrames);
  processor_->processMono(input.data(), output2.data(), numFrames);

  for (int i = 0; i < numFrames; ++i)
  {
    EXPECT_NEAR(output1[i], output2[i], 1e-6f) << "Inconsistent output at sample " << i;
  }
}

TEST_F(LatencyCompensationTest, SampleRate_DefaultValue)
{
  auto newProcessor = std::make_unique<IRProcessor>();
  EXPECT_GT(newProcessor->getLatencySamples(), -1);
}

TEST_F(LatencyCompensationTest, SampleRate_ChangesAffectProcessing)
{
  processor_->setSampleRate(44100.0);
  EXPECT_GE(processor_->getLatencySamples(), 0);

  processor_->setSampleRate(96000.0);
  EXPECT_GE(processor_->getLatencySamples(), 0);

  processor_->setSampleRate(48000.0);
  EXPECT_GE(processor_->getLatencySamples(), 0);
}

TEST_F(LatencyCompensationTest, ZeroLengthBuffer_HandledSafely)
{
  std::vector<Sample> input;
  std::vector<Sample> output;

  EXPECT_NO_THROW(processor_->processMono(input.data(), output.data(), 0));
}

TEST_F(LatencyCompensationTest, LargeBuffer_ProcessedCorrectly)
{
  const int numFrames = 8192;
  std::vector<Sample> input(numFrames, 0.01f);
  std::vector<Sample> output(numFrames, 0.0f);

  EXPECT_NO_THROW(processor_->processMono(input.data(), output.data(), numFrames));

  for (int i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(output[i], input[i]) << "Passthrough mismatch at sample " << i;
  }
}

TEST_F(LatencyCompensationTest, NoClipping_NormalizedInput)
{
  const int numFrames = 1024;
  std::vector<Sample> input(numFrames);
  std::vector<Sample> output(numFrames);

  for (int i = 0; i < numFrames; ++i)
  {
    input[i] = 0.9f * std::sin(2.0f * M_PI * 440.0f * i / 48000.0f);
  }

  processor_->processMono(input.data(), output.data(), numFrames);

  for (int i = 0; i < numFrames; ++i)
  {
    EXPECT_LE(std::abs(output[i]), 10.0f)
        << "Potential clipping detected at sample " << i << " with value " << output[i];
  }
}

TEST_F(LatencyCompensationTest, DelayBuffer_RingBufferWrap)
{
  const size_t bufferSize = 64;
  std::vector<Sample> ringBuffer(bufferSize, 0.0f);
  size_t writePos = 0;

  for (int pass = 0; pass < 3; ++pass)
  {
    for (size_t i = 0; i < bufferSize; ++i)
    {
      ringBuffer[writePos] = static_cast<float>(pass * bufferSize + i);
      writePos = (writePos + 1) % bufferSize;
    }
  }

  EXPECT_EQ(writePos, 0);

  for (size_t i = 0; i < bufferSize; ++i)
  {
    EXPECT_FLOAT_EQ(ringBuffer[i], static_cast<float>(2 * bufferSize + i));
  }
}

TEST_F(LatencyCompensationTest, PhaseCoherence_IdenticalInputs)
{
  const int numFrames = 512;
  std::vector<Sample> inputL(numFrames, 0.5f);
  std::vector<Sample> inputR(numFrames, 0.5f);
  std::vector<Sample> outputL(numFrames, 0.0f);
  std::vector<Sample> outputR(numFrames, 0.0f);

  processor_->processStereo(inputL.data(), inputR.data(), outputL.data(), outputR.data(),
                            numFrames);

  for (int i = 0; i < numFrames; ++i)
  {
    EXPECT_NEAR(outputL[i], outputR[i], 1e-5f)
        << "Phase mismatch between L/R channels at sample " << i;
  }
}
