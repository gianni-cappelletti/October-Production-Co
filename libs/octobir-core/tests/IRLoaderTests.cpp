// clang-format off
// <cstdlib> must precede <convoengine.h> — WDL heapbuf.h/fastqueue.h use
// malloc/free without including <cstdlib> themselves, which fails on GCC/Linux.
#include <cstdlib>
#include <convoengine.h>
// clang-format on

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

// DR_WAV_IMPLEMENTATION is compiled into octobir-core via IRLoader.cpp.
#include "dr_wav.h"
#include "octobir-core/IRLoader.hpp"

using namespace octob;

namespace
{

bool writeTempWavMono(const std::string& path, const std::vector<float>& samples,
                      unsigned int sampleRate)
{
  drwav_data_format fmt;
  fmt.container = drwav_container_riff;
  fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  fmt.channels = 1;
  fmt.sampleRate = sampleRate;
  fmt.bitsPerSample = 32;

  drwav wav;
  if (!drwav_init_file_write(&wav, path.c_str(), &fmt, nullptr))
    return false;

  drwav_write_pcm_frames(&wav, samples.size(), samples.data());
  drwav_uninit(&wav);
  return true;
}

}  // namespace

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
  float irCompensationGain = std::exp(IrCompensationGainDb * DbToLinearScalar);

  EXPECT_NEAR(irCompensationGain, 0.12589254117941673f, 0.00001f);
}

TEST_F(IRLoaderTest, CompensationGain_VerifyDbConversion)
{
  float irCompensationGain = std::exp(IrCompensationGainDb * DbToLinearScalar);

  float linearGainFromPow = std::pow(10.0f, IrCompensationGainDb / 20.0f);

  EXPECT_NEAR(irCompensationGain, linearGainFromPow, 0.0001f);
}

// Minimum phase conversion: a delayed Dirac delta δ[n-K] has a flat magnitude
// spectrum (|H(ω)| = 1), so its minimum phase form is δ[n] — the peak must move
// to sample 0 regardless of the original delay K.
TEST_F(IRLoaderTest, MinimumPhase_DelayedImpulse_PeakAtOrigin)
{
  constexpr int kIRLen = 256;
  constexpr int kDelay = 50;
  constexpr unsigned int kSampleRate = 48000u;

  std::vector<float> ir(kIRLen, 0.0f);
  ir[kDelay] = 1.0f;

  const std::string path = "/tmp/octobir_test_delayed_impulse.wav";
  ASSERT_TRUE(writeTempWavMono(path, ir, kSampleRate));

  IRLoadResult result = loader.loadFromFile(path);
  ASSERT_TRUE(result.success);

  WDL_ImpulseBuffer buf;
  ASSERT_TRUE(loader.resampleAndInitialize(buf, kSampleRate));

  EXPECT_EQ(IRLoader::findPeakSampleIndex(buf), 0);
}

// Minimum phase conversion preserves the magnitude spectrum exactly, so by
// Parseval's theorem the total signal energy must be conserved. Only the phase
// is changed, not the amplitude content.
TEST_F(IRLoaderTest, MinimumPhase_EnergyPreserved)
{
  constexpr int kIRLen = 256;
  constexpr unsigned int kSampleRate = 48000u;

  // Decaying exponential h[n] = 0.9^n — a simple minimum-phase response
  // modified here by adding a 20-sample delay so it is not already minimum phase.
  constexpr int kDelay = 20;
  std::vector<float> ir(kIRLen, 0.0f);
  double originalEnergy = 0.0;
  for (int n = kDelay; n < kIRLen; ++n)
  {
    ir[n] = std::pow(0.9f, n - kDelay);
    originalEnergy += static_cast<double>(ir[n]) * ir[n];
  }

  const std::string path = "/tmp/octobir_test_energy_preservation.wav";
  ASSERT_TRUE(writeTempWavMono(path, ir, kSampleRate));

  IRLoadResult result = loader.loadFromFile(path);
  ASSERT_TRUE(result.success);

  WDL_ImpulseBuffer buf;
  ASSERT_TRUE(loader.resampleAndInitialize(buf, kSampleRate));

  const float compensationGain = std::exp(IrCompensationGainDb * DbToLinearScalar);
  const double expectedEnergy = originalEnergy * compensationGain * compensationGain;

  const WDL_FFT_REAL* data = buf.impulses[0].Get();
  const int len = buf.GetLength();
  double bufEnergy = 0.0;
  for (int i = 0; i < len; ++i)
    bufEnergy += static_cast<double>(data[i]) * data[i];

  EXPECT_NEAR(bufEnergy, expectedEnergy, expectedEnergy * 0.01);
}
