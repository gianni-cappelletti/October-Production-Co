#include <gtest/gtest.h>

#include "octobir-core/IRProcessor.hpp"

using namespace octob;

class DynamicModeTest : public ::testing::Test
{
 protected:
  IRProcessor processor;
};

TEST_F(DynamicModeTest, InputLevelTracking_InitialValue)
{
  EXPECT_FLOAT_EQ(processor.getCurrentInputLevel(), -96.0f);
}

// Without IRs loaded, processMono returns early before updating currentInputLevelDb_.
// This verifies that the level meter is not dirtied by passthrough processing.
TEST_F(DynamicModeTest, InputLevelTracking_UnchangedWithNoIRsLoaded)
{
  constexpr size_t numFrames = 512;
  std::vector<Sample> input(numFrames, 0.9f);
  std::vector<Sample> output(numFrames, 0.0f);

  processor.processMono(input.data(), output.data(), numFrames);

  EXPECT_FLOAT_EQ(processor.getCurrentInputLevel(), -96.0f);
}

TEST_F(DynamicModeTest, CurrentBlend_InitialValue)
{
  EXPECT_FLOAT_EQ(processor.getCurrentBlend(), 0.0f);
}

TEST_F(DynamicModeTest, DetectionMode_DefaultIsPeak)
{
  EXPECT_EQ(processor.getDetectionMode(), 0);
}

TEST_F(DynamicModeTest, DetectionMode_SetAndGet)
{
  processor.setDetectionMode(1);
  EXPECT_EQ(processor.getDetectionMode(), 1);

  processor.setDetectionMode(0);
  EXPECT_EQ(processor.getDetectionMode(), 0);
}

TEST_F(DynamicModeTest, DetectionMode_Clamping)
{
  processor.setDetectionMode(-1);
  EXPECT_EQ(processor.getDetectionMode(), 0);

  processor.setDetectionMode(99);
  EXPECT_EQ(processor.getDetectionMode(), 1);
}

TEST_F(DynamicModeTest, DynamicMode_DisableSnapsCurrentBlendToBlend)
{
  processor.setBlend(0.75f);
  processor.setDynamicModeEnabled(true);
  processor.setDynamicModeEnabled(false);

  EXPECT_FLOAT_EQ(processor.getCurrentBlend(), 0.75f);
}

TEST_F(DynamicModeTest, DynamicMode_DisableSnapsSmoothedBlend)
{
  processor.setBlend(-0.5f);
  processor.setDynamicModeEnabled(true);
  processor.setDynamicModeEnabled(false);

  processor.setBlend(0.3f);
  processor.setDynamicModeEnabled(true);
  processor.setDynamicModeEnabled(false);

  EXPECT_FLOAT_EQ(processor.getCurrentBlend(), 0.3f);
}
