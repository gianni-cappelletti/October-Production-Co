#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "octobass-core/BassProcessor.hpp"

using namespace octob;

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 44100.0;
constexpr size_t kBlockSize = 512;
constexpr size_t kNumBlocks = 16;
constexpr size_t kSettledSample = 8 * kBlockSize;

// Level change of the quarter-gain fixture model: 20 * log10(0.25)
constexpr float kQuarterGainDb = -12.0411998f;

const std::string kDistortionModel = std::string(TEST_DATA_DIR) + "/INPUT_octobass_hm2_a1.nam";
const std::string kQuarterGainModel = std::string(TEST_DATA_DIR) + "/INPUT_linear_quarter_gain.nam";
// Identity Linear model with input_level_dbu 18.0, output_level_dbu 14.0, loudness -21.5
const std::string kCalibrationModel = std::string(TEST_DATA_DIR) + "/INPUT_calibration_linear.nam";
const std::string kSlimmableModel =
    std::string(TEST_DATA_DIR) + "/INPUT_HM2-W OctoBASS distortion 2_a2.nam";

std::unique_ptr<BassProcessor> makeProcessor()
{
  auto proc = std::make_unique<BassProcessor>();
  proc->setSampleRate(kSampleRate);
  proc->setMaxBlockSize(kBlockSize);
  return proc;
}

void load(BassProcessor& proc, NamSlot slot, const std::string& path)
{
  std::string err;
  ASSERT_TRUE(proc.loadNamModel(slot, path, err)) << "Failed to load '" << path << "': " << err;
}

// 1kHz and 2kHz sit well above the default 250Hz crossover, so these tones
// run through the high band and therefore through both NAM slots
std::vector<float> generateSine(float freqHz, float amplitude = 0.3f)
{
  std::vector<float> buf(kNumBlocks * kBlockSize);
  for (size_t i = 0; i < buf.size(); ++i)
    buf[i] = amplitude * static_cast<float>(
                             std::sin(2.0 * kPi * freqHz * static_cast<double>(i) / kSampleRate));
  return buf;
}

std::vector<float> render(BassProcessor& proc, const std::vector<float>& input)
{
  std::vector<float> output(input.size());
  for (size_t offset = 0; offset < input.size(); offset += kBlockSize)
    proc.processMono(input.data() + offset, output.data() + offset, kBlockSize);
  return output;
}

float maxSettledDifference(const std::vector<float>& a, const std::vector<float>& b)
{
  float maxDiff = 0.0f;
  for (size_t i = kSettledSample; i < a.size(); ++i)
    maxDiff = std::max(maxDiff, std::abs(a[i] - b[i]));
  return maxDiff;
}

double settledGainDb(BassProcessor& proc, float toneHz)
{
  const auto input = generateSine(toneHz);
  const auto output = render(proc, input);
  double inputSumSq = 0.0;
  double outputSumSq = 0.0;
  for (size_t i = kSettledSample; i < input.size(); ++i)
  {
    inputSumSq += static_cast<double>(input[i]) * input[i];
    outputSumSq += static_cast<double>(output[i]) * output[i];
  }
  return 10.0 * std::log10(outputSumSq / inputSumSq);
}

}  // namespace

TEST(NamChainTest, BothSlotsStartEmptyAndSwitchedOn)
{
  auto proc = makeProcessor();
  for (const auto slot : {NamSlot::First, NamSlot::Second})
  {
    EXPECT_FALSE(proc->isNamModelLoaded(slot));
    EXPECT_TRUE(proc->getCurrentNamModelPath(slot).empty());
    EXPECT_EQ(proc->getNamQualityLevels(slot), 0);
    EXPECT_TRUE(proc->isNamEnabled(slot));
  }
}

TEST(NamChainTest, SlotsLoadAndClearIndependently)
{
  auto proc = makeProcessor();
  load(*proc, NamSlot::First, kDistortionModel);
  load(*proc, NamSlot::Second, kQuarterGainModel);
  EXPECT_EQ(proc->getCurrentNamModelPath(NamSlot::First), kDistortionModel);
  EXPECT_EQ(proc->getCurrentNamModelPath(NamSlot::Second), kQuarterGainModel);

  proc->clearNamModel(NamSlot::First);
  EXPECT_FALSE(proc->isNamModelLoaded(NamSlot::First));
  EXPECT_TRUE(proc->isNamModelLoaded(NamSlot::Second));
  EXPECT_EQ(proc->getCurrentNamModelPath(NamSlot::Second), kQuarterGainModel);

  load(*proc, NamSlot::First, kDistortionModel);
  proc->clearNamModel(NamSlot::Second);
  EXPECT_TRUE(proc->isNamModelLoaded(NamSlot::First));
  EXPECT_FALSE(proc->isNamModelLoaded(NamSlot::Second));
  EXPECT_EQ(proc->getCurrentNamModelPath(NamSlot::First), kDistortionModel);
}

TEST(NamChainTest, FailedLoadLeavesThatSlotsModelInPlace)
{
  auto proc = makeProcessor();
  load(*proc, NamSlot::Second, kQuarterGainModel);

  std::string err;
  EXPECT_FALSE(proc->loadNamModel(NamSlot::Second, "/nonexistent/path/model.nam", err));
  EXPECT_FALSE(err.empty());
  EXPECT_EQ(proc->getCurrentNamModelPath(NamSlot::Second), kQuarterGainModel);
  EXPECT_FALSE(proc->isNamModelLoaded(NamSlot::First));
}

TEST(NamChainTest, ModelMetadataBelongsToItsSlot)
{
  auto proc = makeProcessor();
  load(*proc, NamSlot::Second, kCalibrationModel);

  EXPECT_EQ(proc->getNamModelMetadata(NamSlot::First), NamModelMetadata{});
  const auto metadata = proc->getNamModelMetadata(NamSlot::Second);
  EXPECT_TRUE(metadata.hasInputLevel);
  EXPECT_DOUBLE_EQ(metadata.inputLevelDbu, 18.0);
}

TEST(NamChainTest, SecondSlotProcessesTheFirstSlotsOutput)
{
  // A distortion model and a plain x0.25 model do not commute, so the order
  // they run in is audible. Each chain is compared against the same
  // distortion model with the x0.25 applied by a known gain stage instead:
  // the high output gain (after the NAM slots) or the high input gain (before)
  const auto input = generateSine(1000.0f);

  auto distortionThenQuarter = makeProcessor();
  load(*distortionThenQuarter, NamSlot::First, kDistortionModel);
  load(*distortionThenQuarter, NamSlot::Second, kQuarterGainModel);

  auto distortionThenOutputTrim = makeProcessor();
  load(*distortionThenOutputTrim, NamSlot::First, kDistortionModel);
  distortionThenOutputTrim->setHighOutputGain(kQuarterGainDb);

  auto quarterThenDistortion = makeProcessor();
  load(*quarterThenDistortion, NamSlot::First, kQuarterGainModel);
  load(*quarterThenDistortion, NamSlot::Second, kDistortionModel);

  auto inputTrimThenDistortion = makeProcessor();
  inputTrimThenDistortion->setHighInputGain(kQuarterGainDb);
  load(*inputTrimThenDistortion, NamSlot::First, kDistortionModel);

  const auto outDistortionThenQuarter = render(*distortionThenQuarter, input);
  const auto outQuarterThenDistortion = render(*quarterThenDistortion, input);

  EXPECT_LT(
      maxSettledDifference(outDistortionThenQuarter, render(*distortionThenOutputTrim, input)),
      1.0e-4f)
      << "Slot 1 then slot 2 must equal slot 1's model followed by slot 2's gain";
  EXPECT_LT(maxSettledDifference(outQuarterThenDistortion, render(*inputTrimThenDistortion, input)),
            1.0e-4f)
      << "Slot 2 must receive slot 1's output";
  EXPECT_GT(maxSettledDifference(outDistortionThenQuarter, outQuarterThenDistortion), 1.0e-2f)
      << "Swapping the slots must change the sound for a nonlinear model";
}

TEST(NamChainTest, EachSlotKeepsItsOwnCalibration)
{
  constexpr float kToneHz = 2000.0f;

  auto uncalibrated = makeProcessor();
  load(*uncalibrated, NamSlot::First, kCalibrationModel);
  load(*uncalibrated, NamSlot::Second, kCalibrationModel);
  const double baselineDb = settledGainDb(*uncalibrated, kToneHz);

  // Slot 2 Calibrated at the default 12 dBu level: 14 - 12 = +2 dB
  auto secondCalibrated = makeProcessor();
  load(*secondCalibrated, NamSlot::First, kCalibrationModel);
  load(*secondCalibrated, NamSlot::Second, kCalibrationModel);
  secondCalibrated->setNamOutputMode(NamSlot::Second, static_cast<int>(NamOutputMode::Calibrated));
  EXPECT_NEAR(settledGainDb(*secondCalibrated, kToneHz) - baselineDb, 2.0, 0.15)
      << "Slot 2's output mode must not be applied to slot 1";

  // Slot 1 input calibration at 24 dBu: 24 - 18 = +6 dB. Slot 2 keeps its own
  // 12 dBu level, so its Calibrated output stays +2 dB (a shared level would
  // turn it into 14 - 24 = -10 dB)
  auto bothCalibrated = makeProcessor();
  load(*bothCalibrated, NamSlot::First, kCalibrationModel);
  load(*bothCalibrated, NamSlot::Second, kCalibrationModel);
  bothCalibrated->setNamCalibrateInput(NamSlot::First, true);
  bothCalibrated->setNamInputCalibrationLevel(NamSlot::First, 24.0f);
  bothCalibrated->setNamOutputMode(NamSlot::Second, static_cast<int>(NamOutputMode::Calibrated));
  EXPECT_NEAR(settledGainDb(*bothCalibrated, kToneHz) - baselineDb, 8.0, 0.15)
      << "Each slot must use its own input calibration level";
}

TEST(NamChainTest, EachSlotKeepsItsOwnQuality)
{
  const auto input = generateSine(1000.0f);

  auto fullQuality = makeProcessor();
  load(*fullQuality, NamSlot::Second, kSlimmableModel);
  EXPECT_EQ(fullQuality->getNamQualityLevels(NamSlot::Second), 2);
  EXPECT_EQ(fullQuality->getNamQualityLevels(NamSlot::First), 0);
  const auto outFull = render(*fullQuality, input);

  auto secondSlotLite = makeProcessor();
  load(*secondSlotLite, NamSlot::Second, kSlimmableModel);
  secondSlotLite->setNamQuality(NamSlot::Second, 0.0f);
  EXPECT_GT(maxSettledDifference(outFull, render(*secondSlotLite, input)), 1.0e-4f)
      << "Slot 2's quality must select a different submodel";

  auto firstSlotLite = makeProcessor();
  load(*firstSlotLite, NamSlot::Second, kSlimmableModel);
  firstSlotLite->setNamQuality(NamSlot::First, 0.0f);
  EXPECT_LT(maxSettledDifference(outFull, render(*firstSlotLite, input)), 1.0e-7f)
      << "Slot 1's quality must not reach the model in slot 2";
}

TEST(NamChainTest, SwitchingASlotOffRemovesOnlyThatSlot)
{
  const auto input = generateSine(1000.0f);

  auto firstOff = makeProcessor();
  load(*firstOff, NamSlot::First, kDistortionModel);
  load(*firstOff, NamSlot::Second, kQuarterGainModel);
  firstOff->setNamEnabled(NamSlot::First, false);

  auto onlyQuarter = makeProcessor();
  load(*onlyQuarter, NamSlot::Second, kQuarterGainModel);

  EXPECT_LT(maxSettledDifference(render(*firstOff, input), render(*onlyQuarter, input)), 1.0e-6f);

  auto secondOff = makeProcessor();
  load(*secondOff, NamSlot::First, kDistortionModel);
  load(*secondOff, NamSlot::Second, kQuarterGainModel);
  secondOff->setNamEnabled(NamSlot::Second, false);

  auto onlyDistortion = makeProcessor();
  load(*onlyDistortion, NamSlot::First, kDistortionModel);

  EXPECT_LT(maxSettledDifference(render(*secondOff, input), render(*onlyDistortion, input)),
            1.0e-6f);
}

TEST(NamChainTest, SlotSwitchedOffStaysOffWhenANewModelIsLoaded)
{
  const auto input = generateSine(1000.0f);

  auto switchedOff = makeProcessor();
  switchedOff->setNamEnabled(NamSlot::Second, false);
  load(*switchedOff, NamSlot::Second, kQuarterGainModel);
  EXPECT_FALSE(switchedOff->isNamEnabled(NamSlot::Second));

  auto empty = makeProcessor();

  EXPECT_LT(maxSettledDifference(render(*switchedOff, input), render(*empty, input)), 1.0e-6f);
}
