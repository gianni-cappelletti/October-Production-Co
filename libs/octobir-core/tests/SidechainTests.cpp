#include <gtest/gtest.h>

#include "octobir-core/IRProcessor.hpp"

using namespace octob;

class SidechainTest : public ::testing::Test
{
 protected:
  IRProcessor processor;
};

TEST_F(SidechainTest, Mono_Sidechain_Passthrough_NoIRsLoaded)
{
  constexpr size_t numFrames = 8;
  Sample input[numFrames] = {1.0f, -0.5f, 0.25f, 0.0f, 0.75f, -0.1f, 0.5f, -0.9f};
  Sample sidechain[numFrames] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
  Sample output[numFrames] = {0.0f};

  processor.processMonoWithSidechain(input, sidechain, output, numFrames);

  for (size_t i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(output[i], input[i]) << "Passthrough mismatch at sample " << i;
  }
}

TEST_F(SidechainTest, Stereo_Sidechain_Passthrough_NoIRsLoaded)
{
  constexpr size_t numFrames = 8;
  Sample inputL[numFrames] = {1.0f, -0.5f, 0.25f, 0.0f, 0.75f, -0.1f, 0.5f, -0.9f};
  Sample inputR[numFrames] = {0.5f, -0.25f, 0.125f, 0.0f, 0.3f, -0.2f, 0.6f, -0.4f};
  Sample sidechainL[numFrames] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
  Sample sidechainR[numFrames] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
  Sample outputL[numFrames] = {0.0f};
  Sample outputR[numFrames] = {0.0f};

  processor.processStereoWithSidechain(inputL, inputR, sidechainL, sidechainR, outputL, outputR,
                                       numFrames);

  for (size_t i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(outputL[i], inputL[i]) << "L passthrough mismatch at sample " << i;
    EXPECT_FLOAT_EQ(outputR[i], inputR[i]) << "R passthrough mismatch at sample " << i;
  }
}

TEST_F(SidechainTest, DualMono_Passthrough_NoIRsLoaded)
{
  constexpr size_t numFrames = 8;
  Sample inputL[numFrames] = {1.0f, -0.5f, 0.25f, 0.0f, 0.75f, -0.1f, 0.5f, -0.9f};
  Sample inputR[numFrames] = {0.5f, -0.25f, 0.125f, 0.0f, 0.3f, -0.2f, 0.6f, -0.4f};
  Sample outputL[numFrames] = {0.0f};
  Sample outputR[numFrames] = {0.0f};

  processor.processDualMono(inputL, inputR, outputL, outputR, numFrames);

  for (size_t i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(outputL[i], inputL[i]) << "L passthrough mismatch at sample " << i;
    EXPECT_FLOAT_EQ(outputR[i], inputR[i]) << "R passthrough mismatch at sample " << i;
  }
}

TEST_F(SidechainTest, DualMono_Sidechain_Passthrough_NoIRsLoaded)
{
  constexpr size_t numFrames = 8;
  Sample inputL[numFrames] = {1.0f, -0.5f, 0.25f, 0.0f, 0.75f, -0.1f, 0.5f, -0.9f};
  Sample inputR[numFrames] = {0.5f, -0.25f, 0.125f, 0.0f, 0.3f, -0.2f, 0.6f, -0.4f};
  Sample sidechainL[numFrames] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
  Sample sidechainR[numFrames] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
  Sample outputL[numFrames] = {0.0f};
  Sample outputR[numFrames] = {0.0f};

  processor.processDualMonoWithSidechain(inputL, inputR, sidechainL, sidechainR, outputL, outputR,
                                         numFrames);

  for (size_t i = 0; i < numFrames; ++i)
  {
    EXPECT_FLOAT_EQ(outputL[i], inputL[i]) << "L passthrough mismatch at sample " << i;
    EXPECT_FLOAT_EQ(outputR[i], inputR[i]) << "R passthrough mismatch at sample " << i;
  }
}
