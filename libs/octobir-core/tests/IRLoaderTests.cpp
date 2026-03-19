#include <gtest/gtest.h>

#include <cmath>

#include "octobir-core/IRLoader.hpp"

using namespace octob;

class IRLoaderTest : public ::testing::Test
{
 protected:
  IRLoader loader;
};

TEST_F(IRLoaderTest, InitialState)
{
  EXPECT_EQ(loader.getIRSampleRate(), 0.0);
  EXPECT_EQ(loader.getNumSamples(), 0u);
  EXPECT_EQ(loader.getNumChannels(), 0);
}

TEST_F(IRLoaderTest, LoadNonexistentFile)
{
  IRLoadResult result = loader.loadFromFile("/nonexistent/file.wav");
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(IRLoaderTest, LoadInvalidFile)
{
  IRLoadResult result = loader.loadFromFile("/etc/passwd");
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(IRLoaderTest, CompensationGain_VerifyValue)
{
  float irCompensationGain = std::exp(kIrCompensationGainDb * kDbToLinearScalar);

  EXPECT_NEAR(irCompensationGain, 0.12589254117941673f, 0.00001f);
}

TEST_F(IRLoaderTest, CompensationGain_VerifyDbConversion)
{
  float irCompensationGain = std::exp(kIrCompensationGainDb * kDbToLinearScalar);

  float linearGainFromPow = std::pow(10.0f, kIrCompensationGainDb / 20.0f);

  EXPECT_NEAR(irCompensationGain, linearGainFromPow, 0.0001f);
}
