#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <set>
#include <vector>

#include "PluginProcessor.h"

using octob::NamSlot;

namespace
{

const juce::String kDataDir = TEST_DATA_DIR;
const juce::String kA1Model = kDataDir + "/INPUT_octobass_hm2_a1.nam";
const juce::String kA2Model = kDataDir + "/INPUT_HM2-W OctoBASS distortion 2_a2.nam";
// Identity Linear model with input_level_dbu 18.0, output_level_dbu 14.0, loudness -21.5
const juce::String kCalibrationModel = kDataDir + "/INPUT_calibration_linear.nam";
const juce::String kImpulseResponse = kDataDir + "/INPUT_ir_a.wav";

struct SlotParamIds
{
  const char* quality;
  const char* calibrateInput;
  const char* inputLevel;
  const char* outputMode;
  const char* on;
  const char* hostNamePrefix;
};

// Slot 1 keeps the IDs that 2.2.0 sessions and automation already refer to
constexpr SlotParamIds kSlot1Params{
    "namQuality", "namCalibrateInput", "namInputCalibrationLevel", "namOutputMode", "namOn",
    "NAM 1"};
constexpr SlotParamIds kSlot2Params{
    "nam2Quality", "nam2CalibrateInput", "nam2InputCalibrationLevel", "nam2OutputMode", "nam2On",
    "NAM 2"};
constexpr std::array<SlotParamIds, 2> kAllSlotParams{kSlot1Params, kSlot2Params};

void setParam(OctoBassProcessor& proc, const char* paramId, float value)
{
  auto* param = proc.getAPVTS().getParameter(paramId);
  ASSERT_NE(param, nullptr) << paramId;
  param->setValueNotifyingHost(param->convertTo0to1(value));
}

float paramValue(OctoBassProcessor& proc, const char* paramId)
{
  auto* value = proc.getAPVTS().getRawParameterValue(paramId);
  EXPECT_NE(value, nullptr) << paramId;
  return value != nullptr ? value->load() : 0.0f;
}

// Restoring a session rebuilds file paths in the platform's native form
// (backslashes on Windows), so paths are compared as files rather than strings
juce::String nativePath(const juce::String& path)
{
  return juce::File(path).getFullPathName();
}

void loadModel(OctoBassProcessor& proc, NamSlot slot, const juce::String& path)
{
  juce::String err;
  ASSERT_TRUE(proc.loadNamModel(slot, path, err)) << err;
}

juce::MemoryBlock saveState(OctoBassProcessor& proc)
{
  juce::MemoryBlock state;
  proc.getStateInformation(state);
  return state;
}

juce::MemoryBlock stateFromTree(const juce::ValueTree& tree)
{
  juce::MemoryBlock state;
  std::unique_ptr<juce::XmlElement> xml(tree.createXml());
  juce::AudioProcessor::copyXmlToBinary(*xml, state);
  return state;
}

// Hosts call setStateInformation and the plugin finishes loading files on the
// message thread; flushing the async update makes that deterministic
void restoreState(OctoBassProcessor& proc, const juce::MemoryBlock& state)
{
  proc.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  proc.handleUpdateNowIfNeeded();
}

constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;

// RMS level change of a 2kHz sine (above the crossover, so it runs through
// the NAM slots) pushed through processBlock, measured after settling
double renderGainDb(OctoBassProcessor& proc)
{
  constexpr int kNumBlocks = 16;
  constexpr int kSkipBlocks = 4;

  juce::AudioBuffer<float> buffer(1, kBlockSize);
  juce::MidiBuffer midi;
  double inputSumSq = 0.0;
  double outputSumSq = 0.0;

  for (int b = 0; b < kNumBlocks; ++b)
  {
    for (int i = 0; i < kBlockSize; ++i)
    {
      const int n = b * kBlockSize + i;
      const float v = 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * 2000.0f *
                                      static_cast<float>(n) / static_cast<float>(kSampleRate));
      buffer.setSample(0, i, v);
      if (b >= kSkipBlocks)
        inputSumSq += static_cast<double>(v) * v;
    }

    proc.processBlock(buffer, midi);

    if (b >= kSkipBlocks)
      for (int i = 0; i < kBlockSize; ++i)
        outputSumSq += static_cast<double>(buffer.getSample(0, i)) * buffer.getSample(0, i);
  }

  return 10.0 * std::log10(outputSumSq / inputSumSq);
}

std::vector<float> renderTone(OctoBassProcessor& proc)
{
  constexpr int kNumBlocks = 8;
  proc.prepareToPlay(kSampleRate, kBlockSize);

  juce::AudioBuffer<float> buffer(1, kBlockSize);
  juce::MidiBuffer midi;
  std::vector<float> output;
  for (int b = 0; b < kNumBlocks; ++b)
  {
    for (int i = 0; i < kBlockSize; ++i)
    {
      const int n = b * kBlockSize + i;
      buffer.setSample(0, i,
                       0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f *
                                       static_cast<float>(n) / static_cast<float>(kSampleRate)));
    }
    proc.processBlock(buffer, midi);
    for (int i = 0; i < kBlockSize; ++i)
      output.push_back(buffer.getSample(0, i));
  }
  return output;
}

}  // namespace

TEST(OctoBassNamSlotTest, EachSlotHasItsOwnSettingsWithDefaults)
{
  OctoBassProcessor proc;
  for (const auto& slot : kAllSlotParams)
  {
    EXPECT_NEAR(paramValue(proc, slot.quality), octob::DefaultNamQuality, 0.001f);
    EXPECT_LT(paramValue(proc, slot.calibrateInput), 0.5f);
    EXPECT_NEAR(paramValue(proc, slot.inputLevel), octob::DefaultNamInputCalibrationLevelDbu,
                0.01f);
    EXPECT_EQ(static_cast<int>(paramValue(proc, slot.outputMode)),
              static_cast<int>(octob::NamOutputMode::Normalized));
    EXPECT_GE(paramValue(proc, slot.on), 0.5f) << slot.on << " must default to on";
  }
}

TEST(OctoBassNamSlotTest, HostShowsWhichSlotEachSettingBelongsTo)
{
  OctoBassProcessor proc;
  for (const auto& slot : kAllSlotParams)
  {
    for (const char* paramId :
         {slot.quality, slot.calibrateInput, slot.inputLevel, slot.outputMode, slot.on})
    {
      auto* param = proc.getAPVTS().getParameter(paramId);
      ASSERT_NE(param, nullptr) << paramId;
      EXPECT_TRUE(param->getName(100).startsWith(slot.hostNamePrefix))
          << paramId << " is shown to the host as '" << param->getName(100) << "'";
    }
  }

  std::set<juce::String> names;
  for (auto* param : proc.getParameters())
    EXPECT_TRUE(names.insert(param->getName(100)).second)
        << "Duplicate host parameter name '" << param->getName(100) << "'";
}

TEST(OctoBassNamSlotTest, SavedSessionRestoresBothSlotsAndTheirSettings)
{
  OctoBassProcessor original;
  original.prepareToPlay(kSampleRate, kBlockSize);
  loadModel(original, NamSlot::First, kA1Model);
  loadModel(original, NamSlot::Second, kCalibrationModel);
  setParam(original, kSlot1Params.on, 0.0f);
  setParam(original, kSlot2Params.quality, 0.25f);
  setParam(original, kSlot2Params.calibrateInput, 1.0f);
  setParam(original, kSlot2Params.inputLevel, 20.0f);
  setParam(original, kSlot2Params.outputMode, static_cast<float>(octob::NamOutputMode::Calibrated));

  OctoBassProcessor restored;
  restored.prepareToPlay(kSampleRate, kBlockSize);
  restoreState(restored, saveState(original));

  EXPECT_EQ(nativePath(restored.getCurrentNamModelPath(NamSlot::First)), nativePath(kA1Model));
  EXPECT_EQ(nativePath(restored.getCurrentNamModelPath(NamSlot::Second)),
            nativePath(kCalibrationModel));
  EXPECT_LT(paramValue(restored, kSlot1Params.on), 0.5f);
  EXPECT_GE(paramValue(restored, kSlot2Params.on), 0.5f);
  EXPECT_NEAR(paramValue(restored, kSlot2Params.quality), 0.25f, 0.02f);
  EXPECT_GE(paramValue(restored, kSlot2Params.calibrateInput), 0.5f);
  EXPECT_NEAR(paramValue(restored, kSlot2Params.inputLevel), 20.0f, 0.05f);
  EXPECT_EQ(static_cast<int>(paramValue(restored, kSlot2Params.outputMode)),
            static_cast<int>(octob::NamOutputMode::Calibrated));
  EXPECT_NEAR(paramValue(restored, kSlot1Params.quality), octob::DefaultNamQuality, 0.001f)
      << "Slot 2's quality must not leak into slot 1";
}

TEST(OctoBassNamSlotTest, SessionFromBeforeTheSecondSlotLoadsIntoTheFirstSlot)
{
  // Shape of a 2.2.0 session: one model path and slot 1's parameter IDs
  juce::ValueTree legacy("OctoBassParams");
  legacy.setProperty("namModelPath", kA1Model, nullptr);
  juce::ValueTree quality("PARAM");
  quality.setProperty("id", "namQuality", nullptr);
  quality.setProperty("value", 0.0f, nullptr);
  legacy.appendChild(quality, nullptr);

  OctoBassProcessor proc;
  proc.prepareToPlay(kSampleRate, kBlockSize);
  restoreState(proc, stateFromTree(legacy));

  EXPECT_EQ(nativePath(proc.getCurrentNamModelPath(NamSlot::First)), nativePath(kA1Model));
  EXPECT_FALSE(proc.isNamModelLoaded(NamSlot::Second));
  EXPECT_NEAR(paramValue(proc, kSlot1Params.quality), 0.0f, 0.001f);
  EXPECT_GE(paramValue(proc, kSlot1Params.on), 0.5f) << "Older sessions never switched a slot off";
  EXPECT_GE(paramValue(proc, kSlot2Params.on), 0.5f);
  EXPECT_NEAR(paramValue(proc, kSlot2Params.quality), octob::DefaultNamQuality, 0.001f);
  EXPECT_EQ(static_cast<int>(paramValue(proc, kSlot2Params.outputMode)),
            static_cast<int>(octob::NamOutputMode::Normalized));
}

TEST(OctoBassNamSlotTest, RestoringASessionEmptiesTheSlotsItDoesNotUse)
{
  OctoBassProcessor sessionWithOnlySecondSlot;
  loadModel(sessionWithOnlySecondSlot, NamSlot::Second, kA2Model);
  const auto session = saveState(sessionWithOnlySecondSlot);

  OctoBassProcessor proc;
  proc.prepareToPlay(kSampleRate, kBlockSize);
  loadModel(proc, NamSlot::First, kA1Model);
  loadModel(proc, NamSlot::Second, kCalibrationModel);
  juce::String err;
  ASSERT_TRUE(proc.loadImpulseResponse(kImpulseResponse, err)) << err;

  restoreState(proc, session);

  EXPECT_FALSE(proc.isNamModelLoaded(NamSlot::First));
  EXPECT_TRUE(proc.getCurrentNamModelPath(NamSlot::First).isEmpty());
  EXPECT_EQ(nativePath(proc.getCurrentNamModelPath(NamSlot::Second)), nativePath(kA2Model));
  EXPECT_TRUE(proc.getCurrentIRPath().isEmpty());
}

TEST(OctoBassNamSlotTest, RestoringASessionWhoseModelFileIsGoneEmptiesThatSlot)
{
  const auto missingModel = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getNonexistentChildFile("octobass-moved-capture", ".nam");
  juce::ValueTree saved("OctoBassParams");
  saved.setProperty("namModelPath", kA1Model, nullptr);
  saved.setProperty("nam2ModelPath", missingModel.getFullPathName(), nullptr);

  OctoBassProcessor proc;
  proc.prepareToPlay(kSampleRate, kBlockSize);
  loadModel(proc, NamSlot::Second, kCalibrationModel);

  restoreState(proc, stateFromTree(saved));

  EXPECT_EQ(nativePath(proc.getCurrentNamModelPath(NamSlot::First)), nativePath(kA1Model));
  EXPECT_FALSE(proc.isNamModelLoaded(NamSlot::Second));
}

TEST(OctoBassNamSlotTest, SecondSlotSettingsReachTheAudio)
{
  const auto measure = [](float outputMode, bool on)
  {
    OctoBassProcessor proc;
    proc.prepareToPlay(kSampleRate, kBlockSize);
    loadModel(proc, NamSlot::Second, kCalibrationModel);
    setParam(proc, kSlot2Params.outputMode, outputMode);
    setParam(proc, kSlot2Params.on, on ? 1.0f : 0.0f);
    return renderGainDb(proc);
  };

  const auto raw = static_cast<float>(octob::NamOutputMode::Raw);
  const auto calibrated = static_cast<float>(octob::NamOutputMode::Calibrated);
  const double rawDb = measure(raw, true);

  // Calibrated at the default 12 dBu input level: 14 - 12 = +2 dB
  EXPECT_NEAR(measure(calibrated, true) - rawDb, 2.0, 0.15);
  EXPECT_NEAR(measure(calibrated, false) - rawDb, 0.0, 0.05)
      << "Switching slot 2 off must remove its calibration gain";
}

TEST(OctoBassNamSlotTest, FirstSlotOnSwitchReachesTheAudio)
{
  const auto measure = [](bool on)
  {
    OctoBassProcessor proc;
    proc.prepareToPlay(kSampleRate, kBlockSize);
    loadModel(proc, NamSlot::First, kCalibrationModel);
    setParam(proc, kSlot1Params.on, on ? 1.0f : 0.0f);
    return renderGainDb(proc);
  };

  // Default Normalized output: -18 - (-21.5) = +3.5 dB while on
  EXPECT_NEAR(measure(true) - measure(false), 3.5, 0.15);
}

TEST(OctoBassNamSlotTest, SecondSlotQualityReachesItsModel)
{
  OctoBassProcessor proc;
  loadModel(proc, NamSlot::Second, kA2Model);
  EXPECT_EQ(proc.getNamQualityLevels(NamSlot::Second), 2);
  EXPECT_EQ(proc.getNamQualityLevels(NamSlot::First), 0);

  const auto fullQuality = renderTone(proc);

  setParam(proc, kSlot2Params.quality, 0.0f);
  proc.handleUpdateNowIfNeeded();
  const auto liteQuality = renderTone(proc);

  ASSERT_EQ(fullQuality.size(), liteQuality.size());
  float maxDiff = 0.0f;
  for (size_t i = 0; i < fullQuality.size(); ++i)
    maxDiff = std::max(maxDiff, std::abs(fullQuality[i] - liteQuality[i]));
  EXPECT_GT(maxDiff, 1e-4f) << "nam2Quality must reach the model in slot 2";
}

TEST(OctoBassNamSlotTest, OnSwitchIsAnOnOffControlHostsCanMapMidiTo)
{
  OctoBassProcessor proc;
  for (const auto& slot : kAllSlotParams)
  {
    auto* param = proc.getAPVTS().getParameter(slot.on);
    ASSERT_NE(param, nullptr) << slot.on;
    EXPECT_TRUE(param->isAutomatable()) << slot.on;
    EXPECT_TRUE(param->isBoolean()) << slot.on;
    EXPECT_TRUE(param->isDiscrete()) << slot.on;
    EXPECT_EQ(param->getNumSteps(), 2) << slot.on;
    EXPECT_EQ(param->getText(0.0f, 100), "Off") << slot.on;
    EXPECT_EQ(param->getText(1.0f, 100), "On") << slot.on;
  }
}

TEST(OctoBassNamSlotTest, FootswitchCcFrom64UpSwitchesASlotOnAndBelow64SwitchesItOff)
{
  for (const auto slot : {NamSlot::First, NamSlot::Second})
  {
    const auto& ids = kAllSlotParams[static_cast<size_t>(slot)];
    // Hosts hand a mapped MIDI CC to the parameter as value / 127
    const auto measure = [&](int ccValue)
    {
      OctoBassProcessor proc;
      proc.prepareToPlay(kSampleRate, kBlockSize);
      loadModel(proc, slot, kCalibrationModel);
      proc.getAPVTS().getParameter(ids.on)->setValueNotifyingHost(static_cast<float>(ccValue) /
                                                                  127.0f);
      return renderGainDb(proc);
    };

    const double offDb = measure(0);
    // Default Normalized output: -18 - (-21.5) = +3.5 dB while on
    EXPECT_NEAR(measure(63) - offDb, 0.0, 0.05) << ids.on << " CC 63 must leave the slot off";
    EXPECT_NEAR(measure(64) - offDb, 3.5, 0.15) << ids.on << " CC 64 must switch the slot on";
    EXPECT_NEAR(measure(127) - offDb, 3.5, 0.15) << ids.on << " CC 127 must switch the slot on";
  }
}
