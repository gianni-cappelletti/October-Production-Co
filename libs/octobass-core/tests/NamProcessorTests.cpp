#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "octobass-core/NamProcessor.hpp"

using namespace octob;

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kBlockSize = 512;

std::vector<float> generateSine(float freqHz, float sampleRate, size_t numSamples,
                                float amplitude = 0.3f)
{
  std::vector<float> buf(numSamples);
  for (size_t i = 0; i < numSamples; ++i)
    buf[i] = amplitude * static_cast<float>(std::sin(2.0 * kPi * freqHz * i / sampleRate));
  return buf;
}
}  // namespace

class NamProcessorTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    proc.setSampleRate(44100.0);
    proc.setMaxBlockSize(kBlockSize);
  }

  NamProcessor proc;
  const std::string wavenetModelPath = std::string(TEST_DATA_DIR) + "/INPUT_VHD.nam";
  // A1 (legacy) model: file version 0.5.4, "WaveNet" architecture.
  const std::string a1ModelPath = std::string(TEST_DATA_DIR) + "/INPUT_octobass_hm2_a1.nam";
  // A2 model: file version 0.7.0, "SlimmableContainer" architecture.
  const std::string a2ModelPath =
      std::string(TEST_DATA_DIR) + "/INPUT_HM2-W OctoBASS distortion 2_a2.nam";

  void expectLoadsAndRuns(const std::string& modelPath)
  {
    std::string err;
    ASSERT_TRUE(proc.loadModel(modelPath, err)) << "Failed to load '" << modelPath << "': " << err;
    EXPECT_TRUE(proc.isModelLoaded());
    EXPECT_EQ(proc.getCurrentModelPath(), modelPath);
    EXPECT_GT(proc.getExpectedSampleRate(), 0.0);

    constexpr size_t kNumSamples = kBlockSize * 4;
    const auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
    std::vector<float> output(kNumSamples);
    for (size_t b = 0; b < kNumSamples / kBlockSize; ++b)
      proc.process(input.data() + b * kBlockSize, output.data() + b * kBlockSize, kBlockSize);

    float peak = 0.0f;
    for (float s : output)
      peak = std::max(peak, std::abs(s));
    EXPECT_GT(peak, 1e-6f) << "NAM output should not be silent after loading model";
  }
};

TEST_F(NamProcessorTest, LoadsWaveNetModelWithoutError)
{
  std::string err;
  const bool loaded = proc.loadModel(wavenetModelPath, err);
  ASSERT_TRUE(loaded) << "WaveNet NAM load failed with error: '" << err << "'. "
                      << "If the error is 'No config parser registered for architecture: WaveNet', "
                      << "the linker has stripped wavenet.o from the static archive and its "
                      << "ConfigParserHelper never ran at startup.";
  EXPECT_TRUE(proc.isModelLoaded());
  EXPECT_EQ(proc.getCurrentModelPath(), wavenetModelPath);
  EXPECT_GT(proc.getExpectedSampleRate(), 0.0);
}

TEST_F(NamProcessorTest, LoadsA1WaveNetModel)
{
  expectLoadsAndRuns(a1ModelPath);
}

TEST_F(NamProcessorTest, LoadsA2SlimmableModel)
{
  expectLoadsAndRuns(a2ModelPath);
}

TEST_F(NamProcessorTest, LoadFailsCleanlyOnMissingFile)
{
  std::string err;
  EXPECT_FALSE(proc.loadModel("/nonexistent/path/missing.nam", err));
  EXPECT_FALSE(err.empty());
  EXPECT_FALSE(proc.isModelLoaded());
}

TEST_F(NamProcessorTest, ProcessingLoadedModelProducesNonSilentOutput)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(wavenetModelPath, err)) << err;

  constexpr size_t kNumSamples = kBlockSize * 4;
  const auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);

  for (size_t b = 0; b < kNumSamples / kBlockSize; ++b)
    proc.process(input.data() + b * kBlockSize, output.data() + b * kBlockSize, kBlockSize);

  float peak = 0.0f;
  for (float s : output)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "NAM output should not be silent after loading model";
}

TEST_F(NamProcessorTest, ProcessBypassesCleanlyWhenNoModelLoaded)
{
  constexpr size_t kNumSamples = kBlockSize;
  const auto input = generateSine(500.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples, 0.0f);

  proc.process(input.data(), output.data(), kNumSamples);

  for (size_t i = 0; i < kNumSamples; ++i)
    EXPECT_FLOAT_EQ(output[i], input[i]);
}

TEST_F(NamProcessorTest, ClearModelResetsState)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(wavenetModelPath, err)) << err;
  ASSERT_TRUE(proc.isModelLoaded());

  proc.clearModel();
  EXPECT_FALSE(proc.isModelLoaded());
  EXPECT_TRUE(proc.getCurrentModelPath().empty());
}

TEST_F(NamProcessorTest, QualityDefaultsToFullAndClamps)
{
  EXPECT_DOUBLE_EQ(proc.getQuality(), 1.0);

  proc.setQuality(2.0);
  EXPECT_DOUBLE_EQ(proc.getQuality(), 1.0);

  proc.setQuality(-0.5);
  EXPECT_DOUBLE_EQ(proc.getQuality(), 0.0);

  proc.setQuality(0.5);
  EXPECT_DOUBLE_EQ(proc.getQuality(), 0.5);
}

TEST_F(NamProcessorTest, QualityAffectsA2SlimmableModelOutput)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(a2ModelPath, err)) << err;

  constexpr size_t kNumSamples = kBlockSize * 4;
  const auto input = generateSine(1000.0f, 44100.0f, kNumSamples);

  auto processAll = [&](std::vector<float>& output)
  {
    for (size_t b = 0; b < kNumSamples / kBlockSize; ++b)
      proc.process(input.data() + b * kBlockSize, output.data() + b * kBlockSize, kBlockSize);
  };

  std::vector<float> fullQuality(kNumSamples);
  proc.setQuality(1.0);
  processAll(fullQuality);

  proc.reset();

  std::vector<float> slimQuality(kNumSamples);
  proc.setQuality(0.0);
  processAll(slimQuality);

  float peakFull = 0.0f;
  float peakSlim = 0.0f;
  float maxDiff = 0.0f;
  for (size_t i = 0; i < kNumSamples; ++i)
  {
    ASSERT_TRUE(std::isfinite(slimQuality[i])) << "Non-finite output at sample " << i;
    peakFull = std::max(peakFull, std::abs(fullQuality[i]));
    peakSlim = std::max(peakSlim, std::abs(slimQuality[i]));
    maxDiff = std::max(maxDiff, std::abs(fullQuality[i] - slimQuality[i]));
  }

  EXPECT_GT(peakFull, 1e-6f) << "Full-quality output should not be silent";
  EXPECT_GT(peakSlim, 1e-6f) << "Slim output should not be silent";
  EXPECT_GT(maxDiff, 1e-9f) << "Slimmest submodel should produce different output than the "
                               "full model, or SetSlimmableSize is not taking effect";
}

TEST_F(NamProcessorTest, QualityIsSafeOnNonSlimmableModel)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(wavenetModelPath, err)) << err;

  proc.setQuality(0.25);
  EXPECT_DOUBLE_EQ(proc.getQuality(), 0.25);

  constexpr size_t kNumSamples = kBlockSize;
  const auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  proc.process(input.data(), output.data(), kNumSamples);

  float peak = 0.0f;
  for (float s : output)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "Non-slimmable model must keep processing after a quality change";
}

TEST_F(NamProcessorTest, QualityPersistsAcrossModelLoads)
{
  proc.setQuality(0.0);

  // Quality set before loading must apply to the freshly loaded model
  std::string err;
  ASSERT_TRUE(proc.loadModel(a2ModelPath, err)) << err;
  EXPECT_DOUBLE_EQ(proc.getQuality(), 0.0);

  constexpr size_t kNumSamples = kBlockSize;
  const auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  proc.process(input.data(), output.data(), kNumSamples);

  float peak = 0.0f;
  for (float s : output)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "Slimmest model should still produce output";
}
