#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
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

TEST_F(NamProcessorTest, LoadFailsCleanlyOnInvalidModelContent)
{
  const auto writeTempModel = [](const char* name, const char* content)
  {
    const auto path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream out(path);
    out << content;
    return path;
  };

  const std::string notJsonPath =
      writeTempModel("octobass_not_json.nam", "this is not json at all");
  std::string err;
  EXPECT_FALSE(proc.loadModel(notJsonPath, err)) << "Non-JSON content must be rejected";
  EXPECT_FALSE(err.empty());
  EXPECT_FALSE(proc.isModelLoaded());

  const std::string badArchPath = writeTempModel(
      "octobass_bad_arch.nam", R"({"architecture": "NotARealArchitecture", "config": {}})");
  err.clear();
  EXPECT_FALSE(proc.loadModel(badArchPath, err)) << "Unknown architectures must be rejected";
  EXPECT_FALSE(err.empty());
  EXPECT_FALSE(proc.isModelLoaded());

  std::filesystem::remove(notJsonPath);
  std::filesystem::remove(badArchPath);
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

TEST_F(NamProcessorTest, QualityLevelsAreZeroWithoutModel)
{
  EXPECT_EQ(proc.getNumQualityLevels(), 0);
}

TEST_F(NamProcessorTest, QualityLevelsAreOneForNonSlimmableModels)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(wavenetModelPath, err)) << err;
  EXPECT_EQ(proc.getNumQualityLevels(), 1);

  ASSERT_TRUE(proc.loadModel(a1ModelPath, err)) << err;
  EXPECT_EQ(proc.getNumQualityLevels(), 1);
}

TEST_F(NamProcessorTest, QualityLevelsMatchA2SubmodelCount)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(a2ModelPath, err)) << err;
  EXPECT_EQ(proc.getNumQualityLevels(), 2)
      << "The A2 container has two submodels (max_value 0.5 and 1.0), so it "
         "must report two quality levels";
}

TEST_F(NamProcessorTest, QualityLevelsResetOnClearModel)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(a2ModelPath, err)) << err;
  ASSERT_EQ(proc.getNumQualityLevels(), 2);

  proc.clearModel();
  EXPECT_EQ(proc.getNumQualityLevels(), 0);
}

class NamCalibrationTest : public NamProcessorTest
{
 protected:
  // Identity Linear model (1-tap, weight 1.0) with full level metadata:
  // input_level_dbu 18.0, output_level_dbu 14.0, loudness -21.5
  const std::string calibrationModelPath =
      std::string(TEST_DATA_DIR) + "/INPUT_calibration_linear.nam";

  static constexpr size_t kNumBlocks = 8;
  // The 5ms gain ramp settles well inside one 512-sample block at 44.1kHz;
  // measuring the last block keeps every assertion on steady-state output
  static constexpr size_t kMeasuredBlock = kNumBlocks - 1;

  float steadyStateRms(std::vector<float>& output, const std::vector<float>& input)
  {
    for (size_t b = 0; b < kNumBlocks; ++b)
      proc.process(input.data() + b * kBlockSize, output.data() + b * kBlockSize, kBlockSize);

    double sumSquares = 0.0;
    for (size_t i = kMeasuredBlock * kBlockSize; i < kNumBlocks * kBlockSize; ++i)
      sumSquares += static_cast<double>(output[i]) * output[i];
    return static_cast<float>(std::sqrt(sumSquares / kBlockSize));
  }

  float gainVersusUncalibrated(float rmsCalibrated, const std::vector<float>& input)
  {
    double sumSquares = 0.0;
    for (size_t i = kMeasuredBlock * kBlockSize; i < kNumBlocks * kBlockSize; ++i)
      sumSquares += static_cast<double>(input[i]) * input[i];
    const float rmsInput = static_cast<float>(std::sqrt(sumSquares / kBlockSize));
    return rmsCalibrated / rmsInput;
  }
};

TEST_F(NamCalibrationTest, MetadataEmptyWhenNoModel)
{
  EXPECT_EQ(proc.getModelMetadata(), NamModelMetadata{});
}

TEST_F(NamCalibrationTest, MetadataExposedImmediatelyAfterLoad)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(calibrationModelPath, err)) << err;

  // No process() call yet: the staged (pending) model must already report
  const auto metadata = proc.getModelMetadata();
  EXPECT_TRUE(metadata.hasInputLevel);
  EXPECT_DOUBLE_EQ(metadata.inputLevelDbu, 18.0);
  EXPECT_TRUE(metadata.hasOutputLevel);
  EXPECT_DOUBLE_EQ(metadata.outputLevelDbu, 14.0);
  EXPECT_TRUE(metadata.hasLoudness);
  EXPECT_DOUBLE_EQ(metadata.loudnessDb, -21.5);
}

TEST_F(NamCalibrationTest, MetadataReportsLoudnessOnlyModels)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(wavenetModelPath, err)) << err;

  const auto metadata = proc.getModelMetadata();
  EXPECT_FALSE(metadata.hasInputLevel);
  EXPECT_FALSE(metadata.hasOutputLevel);
  EXPECT_TRUE(metadata.hasLoudness);
  EXPECT_NEAR(metadata.loudnessDb, -19.875, 0.001);
}

TEST_F(NamCalibrationTest, MetadataClearedAfterClearModel)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(calibrationModelPath, err)) << err;
  proc.clearModel();
  EXPECT_EQ(proc.getModelMetadata(), NamModelMetadata{});
}

TEST_F(NamCalibrationTest, InputCalibrationGainMatchesFormula)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(calibrationModelPath, err)) << err;

  proc.setCalibrateInput(true);
  proc.setInputCalibrationLevel(24.0f);  // 24 - 18 = +6 dB through the identity model

  const auto input = generateSine(1000.0f, 44100.0f, kNumBlocks * kBlockSize, 0.1f);
  std::vector<float> output(input.size());
  const float gain = gainVersusUncalibrated(steadyStateRms(output, input), input);
  EXPECT_NEAR(gain, std::pow(10.0f, 6.0f / 20.0f), 0.01f * gain);
}

TEST_F(NamCalibrationTest, InputCalibrationInertWithoutInputLevelMetadata)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(wavenetModelPath, err)) << err;

  const auto input = generateSine(1000.0f, 44100.0f, kNumBlocks * kBlockSize, 0.1f);
  std::vector<float> uncalibrated(input.size());
  steadyStateRms(uncalibrated, input);

  proc.reset();
  proc.setCalibrateInput(true);
  proc.setInputCalibrationLevel(30.0f);
  std::vector<float> calibrated(input.size());
  steadyStateRms(calibrated, input);

  for (size_t i = 0; i < input.size(); ++i)
    ASSERT_FLOAT_EQ(calibrated[i], uncalibrated[i]) << "at sample " << i;
}

TEST_F(NamCalibrationTest, OutputModeRawLeavesIdentityModelAtUnity)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(calibrationModelPath, err)) << err;
  proc.setOutputMode(NamOutputMode::Raw);

  const auto input = generateSine(1000.0f, 44100.0f, kNumBlocks * kBlockSize, 0.1f);
  std::vector<float> output(input.size());
  const float gain = gainVersusUncalibrated(steadyStateRms(output, input), input);
  EXPECT_NEAR(gain, 1.0f, 0.001f);
}

TEST_F(NamCalibrationTest, OutputModeNormalizedUsesLoudness)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(calibrationModelPath, err)) << err;
  proc.setOutputMode(NamOutputMode::Normalized);  // -18 - (-21.5) = +3.5 dB

  const auto input = generateSine(1000.0f, 44100.0f, kNumBlocks * kBlockSize, 0.1f);
  std::vector<float> output(input.size());
  const float gain = gainVersusUncalibrated(steadyStateRms(output, input), input);
  EXPECT_NEAR(gain, std::pow(10.0f, 3.5f / 20.0f), 0.01f * gain);
}

TEST_F(NamCalibrationTest, OutputModeCalibratedUsesOutputLevel)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(calibrationModelPath, err)) << err;
  proc.setOutputMode(NamOutputMode::Calibrated);

  proc.setInputCalibrationLevel(20.0f);  // 14 - 20 = -6 dB
  const auto input = generateSine(1000.0f, 44100.0f, kNumBlocks * kBlockSize, 0.1f);
  std::vector<float> output(input.size());
  float gain = gainVersusUncalibrated(steadyStateRms(output, input), input);
  EXPECT_NEAR(gain, std::pow(10.0f, -6.0f / 20.0f), 0.01f * gain);

  proc.reset();
  proc.setInputCalibrationLevel(12.0f);  // default level: 14 - 12 = +2 dB
  gain = gainVersusUncalibrated(steadyStateRms(output, input), input);
  EXPECT_NEAR(gain, std::pow(10.0f, 2.0f / 20.0f), 0.01f * gain);
}

TEST_F(NamCalibrationTest, CalibrationGainRampIsClickFree)
{
  std::string err;
  ASSERT_TRUE(proc.loadModel(calibrationModelPath, err)) << err;

  // DC through the identity model isolates the gain trajectory from the
  // signal itself: any step in the output is a step in the gain
  const std::vector<float> input(kNumBlocks * kBlockSize, 0.1f);
  std::vector<float> output(input.size());

  proc.process(input.data(), output.data(), kBlockSize);
  proc.setCalibrateInput(true);
  proc.setInputCalibrationLevel(44.0f);  // 44 - 18 = +26 dB, a x20 linear step
  for (size_t b = 1; b < kNumBlocks; ++b)
    proc.process(input.data() + b * kBlockSize, output.data() + b * kBlockSize, kBlockSize);

  // 5ms one-pole at 44.1kHz moves the gain by at most ~0.9% of the remaining
  // 1.9 linear range per sample on a 0.1 signal; a hard step would jump 1.9
  float maxDelta = 0.0f;
  for (size_t i = 1; i < output.size(); ++i)
    maxDelta = std::max(maxDelta, std::abs(output[i] - output[i - 1]));
  EXPECT_LT(maxDelta, 0.05f);
  EXPECT_NEAR(output.back(), 0.1f * std::pow(10.0f, 26.0f / 20.0f), 0.01f);
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
