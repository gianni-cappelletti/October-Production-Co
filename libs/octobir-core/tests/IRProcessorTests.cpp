#include <gtest/gtest.h>

#include <cmath>

#include "octobir-core/IRProcessor.hpp"

using namespace octob;

class IRProcessorTest : public ::testing::Test
{
 protected:
  IRProcessor processor;
};

TEST_F(IRProcessorTest, ParameterClamping_Blend)
{
  processor.setBlend(2.0f);
  EXPECT_FLOAT_EQ(processor.getBlend(), 1.0f);

  processor.setBlend(-2.0f);
  EXPECT_FLOAT_EQ(processor.getBlend(), -1.0f);

  processor.setBlend(0.5f);
  EXPECT_FLOAT_EQ(processor.getBlend(), 0.5f);
}

TEST_F(IRProcessorTest, ParameterClamping_LowBlend)
{
  processor.setLowBlend(2.0f);
  EXPECT_FLOAT_EQ(processor.getLowBlend(), 1.0f);

  processor.setLowBlend(-2.0f);
  EXPECT_FLOAT_EQ(processor.getLowBlend(), -1.0f);

  processor.setLowBlend(-0.5f);
  EXPECT_FLOAT_EQ(processor.getLowBlend(), -0.5f);
}

TEST_F(IRProcessorTest, ParameterClamping_HighBlend)
{
  processor.setHighBlend(2.0f);
  EXPECT_FLOAT_EQ(processor.getHighBlend(), 1.0f);

  processor.setHighBlend(-2.0f);
  EXPECT_FLOAT_EQ(processor.getHighBlend(), -1.0f);

  processor.setHighBlend(0.8f);
  EXPECT_FLOAT_EQ(processor.getHighBlend(), 0.8f);
}

TEST_F(IRProcessorTest, ParameterClamping_Threshold)
{
  processor.setThreshold(10.0f);
  EXPECT_FLOAT_EQ(processor.getThreshold(), 0.0f);

  processor.setThreshold(-80.0f);
  EXPECT_FLOAT_EQ(processor.getThreshold(), -60.0f);

  processor.setThreshold(-30.0f);
  EXPECT_FLOAT_EQ(processor.getThreshold(), -30.0f);
}

TEST_F(IRProcessorTest, ParameterClamping_RangeDb)
{
  processor.setRangeDb(100.0f);
  EXPECT_FLOAT_EQ(processor.getRangeDb(), 60.0f);

  processor.setRangeDb(0.5f);
  EXPECT_FLOAT_EQ(processor.getRangeDb(), 1.0f);

  processor.setRangeDb(20.0f);
  EXPECT_FLOAT_EQ(processor.getRangeDb(), 20.0f);
}

TEST_F(IRProcessorTest, ParameterClamping_KneeWidthDb)
{
  processor.setKneeWidthDb(30.0f);
  EXPECT_FLOAT_EQ(processor.getKneeWidthDb(), 20.0f);

  processor.setKneeWidthDb(-5.0f);
  EXPECT_FLOAT_EQ(processor.getKneeWidthDb(), 0.0f);

  processor.setKneeWidthDb(10.0f);
  EXPECT_FLOAT_EQ(processor.getKneeWidthDb(), 10.0f);
}

TEST_F(IRProcessorTest, ParameterClamping_AttackTime)
{
  processor.setAttackTime(2000.0f);
  EXPECT_FLOAT_EQ(processor.getAttackTime(), 500.0f);

  processor.setAttackTime(0.5f);
  EXPECT_FLOAT_EQ(processor.getAttackTime(), 1.0f);

  processor.setAttackTime(100.0f);
  EXPECT_FLOAT_EQ(processor.getAttackTime(), 100.0f);
}

TEST_F(IRProcessorTest, ParameterClamping_ReleaseTime)
{
  processor.setReleaseTime(2000.0f);
  EXPECT_FLOAT_EQ(processor.getReleaseTime(), 1000.0f);

  processor.setReleaseTime(0.5f);
  EXPECT_FLOAT_EQ(processor.getReleaseTime(), 1.0f);

  processor.setReleaseTime(250.0f);
  EXPECT_FLOAT_EQ(processor.getReleaseTime(), 250.0f);
}

TEST_F(IRProcessorTest, ParameterClamping_IRATrimGain)
{
  processor.setIRATrimGain(20.0f);
  EXPECT_FLOAT_EQ(processor.getIRATrimGain(), 12.0f);

  processor.setIRATrimGain(-20.0f);
  EXPECT_FLOAT_EQ(processor.getIRATrimGain(), -12.0f);

  processor.setIRATrimGain(6.0f);
  EXPECT_FLOAT_EQ(processor.getIRATrimGain(), 6.0f);
}

TEST_F(IRProcessorTest, ParameterClamping_IRBTrimGain)
{
  processor.setIRBTrimGain(20.0f);
  EXPECT_FLOAT_EQ(processor.getIRBTrimGain(), 12.0f);

  processor.setIRBTrimGain(-20.0f);
  EXPECT_FLOAT_EQ(processor.getIRBTrimGain(), -12.0f);

  processor.setIRBTrimGain(-3.0f);
  EXPECT_FLOAT_EQ(processor.getIRBTrimGain(), -3.0f);
}

TEST_F(IRProcessorTest, ParameterClamping_OutputGain)
{
  processor.setOutputGain(30.0f);
  EXPECT_FLOAT_EQ(processor.getOutputGain(), 24.0f);

  processor.setOutputGain(-30.0f);
  EXPECT_FLOAT_EQ(processor.getOutputGain(), -24.0f);

  processor.setOutputGain(6.0f);
  EXPECT_FLOAT_EQ(processor.getOutputGain(), 6.0f);
}

TEST_F(IRProcessorTest, OutputGain_AppliesCorrectGain)
{
  processor.setOutputGain(6.0f);
  constexpr float expectedGain = 1.9952623149688796f;

  constexpr size_t numFrames = 4;
  Sample input[numFrames] = {1.0f, -0.5f, 0.25f, 0.0f};
  Sample output[numFrames] = {0.0f};

  processor.processMono(input, output, numFrames);

  for (size_t i = 0; i < numFrames; ++i)
  {
    EXPECT_NEAR(output[i], input[i] * expectedGain, 0.001f);
  }
}

TEST_F(IRProcessorTest, OutputGain_ZeroGainProducesSameOutput)
{
  processor.setOutputGain(0.0f);

  constexpr size_t numFrames = 4;
  Sample input[numFrames] = {1.0f, -0.5f, 0.25f, 0.0f};
  Sample output[numFrames] = {0.0f};

  processor.processMono(input, output, numFrames);

  for (size_t i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(output[i], input[i]);
  }
}

TEST_F(IRProcessorTest, StateManagement_InitialState)
{
  EXPECT_FALSE(processor.isIR1Loaded());
  EXPECT_FALSE(processor.isIR2Loaded());
  EXPECT_EQ(processor.getCurrentIR1Path(), "");
  EXPECT_EQ(processor.getCurrentIR2Path(), "");
  EXPECT_FLOAT_EQ(processor.getBlend(), 0.0f);
  EXPECT_FALSE(processor.getDynamicModeEnabled());
  EXPECT_FALSE(processor.getSidechainEnabled());
}

TEST_F(IRProcessorTest, DynamicMode_DisabledSetsCurrentBlendToBlend)
{
  processor.setBlend(0.5f);
  processor.setDynamicModeEnabled(true);
  processor.setDynamicModeEnabled(false);

  EXPECT_FLOAT_EQ(processor.getCurrentBlend(), 0.5f);
}

TEST_F(IRProcessorTest, Passthrough_NoIRsLoaded)
{
  constexpr size_t numFrames = 4;
  Sample input[numFrames] = {1.0f, -0.5f, 0.25f, 0.0f};
  Sample output[numFrames] = {0.0f};

  processor.processMono(input, output, numFrames);

  for (size_t i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(output[i], input[i]);
  }
}

TEST_F(IRProcessorTest, Passthrough_Stereo_NoIRsLoaded)
{
  constexpr size_t numFrames = 4;
  Sample inputL[numFrames] = {1.0f, -0.5f, 0.25f, 0.0f};
  Sample inputR[numFrames] = {0.5f, -0.25f, 0.125f, 0.0f};
  Sample outputL[numFrames] = {0.0f};
  Sample outputR[numFrames] = {0.0f};

  processor.processStereo(inputL, inputR, outputL, outputR, numFrames);

  for (size_t i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(outputL[i], inputL[i]);
    EXPECT_FLOAT_EQ(outputR[i], inputR[i]);
  }
}
