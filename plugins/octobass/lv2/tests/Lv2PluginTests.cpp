#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "AllocationTracker.hpp"
#include "FakeLv2Host.hpp"
#include "Ports.hpp"
#include "ReferenceChain.hpp"
#include "Uris.hpp"

using namespace octob;
using namespace octob::lv2;
using namespace octob::lv2::test;
using namespace octob::test;

namespace
{

constexpr double TestSampleRate = 48000.0;
constexpr size_t TestBlockSize = 256;
constexpr double SignalSeconds = 0.5;

const std::string SlimmableModel = testDataPath("INPUT_HM2-W OctoBASS distortion 2_a2.nam");
const std::string SecondModel = testDataPath("INPUT_VHD.nam");
const std::string CabinetIr = testDataPath("INPUT_ir_a.wav");

std::vector<float> firstBlock(const std::vector<float>& signal)
{
  return {signal.begin(), signal.begin() + TestBlockSize};
}

std::vector<float> afterFirstBlock(const std::vector<float>& signal)
{
  return {signal.begin() + TestBlockSize, signal.end()};
}

std::vector<float> concat(std::vector<float> head, const std::vector<float>& tail)
{
  head.insert(head.end(), tail.begin(), tail.end());
  return head;
}

ControlValues hostControls(const FakeLv2Host& host)
{
  ControlValues controls{};
  for (PortIndex index = 0; index < port::NumPorts; ++index)
    controls[index] = host.control(index);
  return controls;
}

::testing::AssertionResult sameAudio(const std::vector<float>& actual,
                                     const std::vector<float>& expected, float tolerance = 0.0f)
{
  if (actual.size() != expected.size())
    return ::testing::AssertionFailure()
           << "length " << actual.size() << " != expected " << expected.size();

  for (size_t i = 0; i < actual.size(); ++i)
  {
    if (!std::isfinite(actual[i]))
      return ::testing::AssertionFailure() << "non-finite sample at " << i;
    if (std::fabs(actual[i] - expected[i]) > tolerance)
      return ::testing::AssertionFailure()
             << "sample " << i << ": " << actual[i] << " != expected " << expected[i];
  }
  return ::testing::AssertionSuccess();
}

std::vector<float> loadThroughPlugin(FakeLv2Host& host, const std::vector<float>& input,
                                     const std::vector<std::pair<FileSlot, std::string>>& files)
{
  for (const auto& [slot, path] : files)
    host.sendFile(slot, path);
  auto head = host.process(firstBlock(input), TestBlockSize);
  host.runWorker();
  return concat(std::move(head), host.process(afterFirstBlock(input), TestBlockSize));
}

std::string notifiedPath(const std::vector<FileNotification>& notifications, FileSlot slot)
{
  std::string path = "<not notified>";
  for (const auto& notification : notifications)
    if (notification.propertyUri == fileParameter(slot).uri)
      path = notification.path;
  return path;
}

}  // namespace

TEST(Lv2Descriptor, ExposesOneOctoBassPlugin)
{
  const LV2_Descriptor* descriptor = lv2_descriptor(0);
  ASSERT_NE(descriptor, nullptr);
  EXPECT_STREQ(descriptor->URI, PluginUri);
  EXPECT_EQ(lv2_descriptor(1), nullptr);
}

TEST(Lv2Instantiation, RefusesHostWithoutUridMap)
{
  HostConfig config;
  config.provideUridMap = false;
  const FakeLv2Host host(config);
  EXPECT_FALSE(host.isInstantiated());
}

TEST(Lv2Instantiation, RefusesHostWithoutWorker)
{
  HostConfig config;
  config.provideWorker = false;
  const FakeLv2Host host(config);
  EXPECT_FALSE(host.isInstantiated());
}

TEST(Lv2Instantiation, RunsWithoutOptionalFeatures)
{
  HostConfig config;
  config.provideLog = false;
  config.provideOptions = false;
  FakeLv2Host host(config);
  ASSERT_TRUE(host.isInstantiated());

  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  ReferenceChain reference(TestSampleRate, TestBlockSize);
  EXPECT_TRUE(sameAudio(host.process(input, TestBlockSize),
                        reference.process(input, TestBlockSize, defaultControls())));
}

TEST(Lv2Processing, DefaultControlsMatchCoreDrivenLikeJuce)
{
  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);

  ReferenceChain reference(TestSampleRate, TestBlockSize);
  EXPECT_TRUE(sameAudio(host.process(input, TestBlockSize),
                        reference.process(input, TestBlockSize, defaultControls())));
}

struct ControlCase
{
  std::string name;
  std::vector<std::pair<PortIndex, float>> values;
};

void PrintTo(const ControlCase& controlCase, std::ostream* stream)
{
  *stream << controlCase.name;
}

class Lv2ControlTest : public ::testing::TestWithParam<ControlCase>
{
};

TEST_P(Lv2ControlTest, ControlDrivesCoreLikeJuce)
{
  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  for (const auto& [index, value] : GetParam().values)
    host.setControl(index, value);

  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  const auto expectedControls = hostControls(host);
  ReferenceChain reference(TestSampleRate, TestBlockSize);
  const auto expected = reference.process(firstBlock(input), TestBlockSize, expectedControls);
  reference.loadNamModel(NamSlot::First, SlimmableModel);
  const auto expectedTail =
      reference.process(afterFirstBlock(input), TestBlockSize, expectedControls);

  const auto actual = loadThroughPlugin(host, input, {{FileSlot::Nam1, SlimmableModel}});
  EXPECT_TRUE(sameAudio(actual, concat(expected, expectedTail)));
}

INSTANTIATE_TEST_SUITE_P(
    EveryControlGroup, Lv2ControlTest,
    ::testing::Values(ControlCase{"Crossover", {{port::Crossover, 120.0f}}},
                      ControlCase{"Squash", {{port::Squash, 75.0f}}},
                      ControlCase{"CompressionMode",
                                  {{port::Squash, 50.0f}, {port::CompressionMode, 2.0f}}},
                      ControlCase{"LowBandLevel", {{port::LowBandLevel, -6.0f}}},
                      ControlCase{"HighInputGain", {{port::HighInputGain, 6.0f}}},
                      ControlCase{"HighOutputGain", {{port::HighOutputGain, -3.0f}}},
                      ControlCase{"OutputGain", {{port::OutputGain, -4.5f}}},
                      ControlCase{"DryWet", {{port::DryWet, 40.0f}}},
                      ControlCase{"Gate", {{port::GateThreshold, -30.0f}}},
                      ControlCase{"HighBlend", {{port::HighBandMix, 30.0f}}},
                      ControlCase{"LowSolo", {{port::LowBandSolo, 1.0f}}},
                      ControlCase{"HighSolo", {{port::HighBandSolo, 1.0f}}},
                      ControlCase{"NamOff", {{port::namOn(NamSlot::First), 0.0f}}},
                      ControlCase{"NamCalibration",
                                  {{port::namCalibrateInput(NamSlot::First), 1.0f},
                                   {port::namInputLevel(NamSlot::First), 4.0f},
                                   {port::namOutputMode(NamSlot::First), 2.0f}}},
                      ControlCase{"NamRawOutput", {{port::namOutputMode(NamSlot::First), 0.0f}}}),
    [](const ::testing::TestParamInfo<ControlCase>& info) { return info.param.name; });

TEST(Lv2Solo, NewlyEngagedSoloWinsWhenBothAreOn)
{
  for (const bool lowFirst : {true, false})
  {
    FakeLv2Host host;
    ASSERT_TRUE(host.isInstantiated());
    const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
    const PortIndex first = lowFirst ? port::LowBandSolo : port::HighBandSolo;
    const PortIndex second = lowFirst ? port::HighBandSolo : port::LowBandSolo;

    host.setControl(first, 1.0f);
    auto actual = host.process(firstBlock(input), TestBlockSize);
    host.setControl(second, 1.0f);
    actual = concat(std::move(actual), host.process(afterFirstBlock(input), TestBlockSize));

    ReferenceChain reference(TestSampleRate, TestBlockSize);
    auto controls = defaultControls();
    controls[first] = 1.0f;
    auto expected = reference.process(firstBlock(input), TestBlockSize, controls);
    controls[first] = 0.0f;
    controls[second] = 1.0f;
    expected = concat(std::move(expected),
                      reference.process(afterFirstBlock(input), TestBlockSize, controls));

    EXPECT_TRUE(sameAudio(actual, expected)) << (lowFirst ? "low then high" : "high then low");
  }
}

TEST(Lv2Files, PatchSetLoadsModelsAndIrThroughWorker)
{
  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);

  const auto actual = loadThroughPlugin(host, input,
                                        {{FileSlot::Nam1, SlimmableModel},
                                         {FileSlot::Nam2, SecondModel},
                                         {FileSlot::CabinetIr, CabinetIr}});

  ReferenceChain reference(TestSampleRate, TestBlockSize);
  auto expected = reference.process(firstBlock(input), TestBlockSize, defaultControls());
  reference.loadNamModel(NamSlot::First, SlimmableModel);
  reference.loadNamModel(NamSlot::Second, SecondModel);
  reference.loadImpulseResponse(CabinetIr);
  expected = concat(std::move(expected),
                    reference.process(afterFirstBlock(input), TestBlockSize, defaultControls()));

  EXPECT_TRUE(sameAudio(actual, expected));

  const auto notifications = host.takeNotifications();
  EXPECT_EQ(notifiedPath(notifications, FileSlot::Nam1), SlimmableModel);
  EXPECT_EQ(notifiedPath(notifications, FileSlot::Nam2), SecondModel);
  EXPECT_EQ(notifiedPath(notifications, FileSlot::CabinetIr), CabinetIr);
}

TEST(Lv2Files, LatencyPortMatchesCoreLatency)
{
  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  EXPECT_EQ(host.control(port::Latency), 0.0f);

  loadThroughPlugin(host, input, {{FileSlot::CabinetIr, CabinetIr}});

  ReferenceChain reference(TestSampleRate, TestBlockSize);
  reference.loadImpulseResponse(CabinetIr);
  reference.process(firstBlock(input), TestBlockSize, defaultControls());
  EXPECT_EQ(host.control(port::Latency), static_cast<float>(reference.latencySamples()));
}

TEST(Lv2Files, EmptyPathClearsSlot)
{
  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  loadThroughPlugin(host, input,
                    {{FileSlot::Nam1, SlimmableModel}, {FileSlot::CabinetIr, CabinetIr}});
  host.takeNotifications();

  const auto actual =
      loadThroughPlugin(host, input, {{FileSlot::Nam1, ""}, {FileSlot::CabinetIr, ""}});

  EXPECT_EQ(host.control(port::Latency), 0.0f);
  const auto notifications = host.takeNotifications();
  EXPECT_EQ(notifiedPath(notifications, FileSlot::Nam1), "");
  EXPECT_EQ(notifiedPath(notifications, FileSlot::CabinetIr), "");

  ReferenceChain cleared(TestSampleRate, TestBlockSize);
  const auto clearedOutput = cleared.process(input, TestBlockSize, defaultControls());
  const std::vector<float> settledActual(actual.end() - TestBlockSize, actual.end());
  const std::vector<float> settledExpected(clearedOutput.end() - TestBlockSize,
                                           clearedOutput.end());
  EXPECT_TRUE(sameAudio(settledActual, settledExpected, 1e-3f));
}

TEST(Lv2Files, InvalidPathsLeaveSlotUnchangedAndAreLogged)
{
  const std::vector<std::string> invalidPaths = {
      "relative/model.nam",
      CabinetIr,
      testDataPath("does_not_exist.nam"),
  };

  for (const auto& invalidPath : invalidPaths)
  {
    FakeLv2Host host;
    ASSERT_TRUE(host.isInstantiated());
    const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
    loadThroughPlugin(host, input, {{FileSlot::Nam1, SlimmableModel}});

    ReferenceChain reference(TestSampleRate, TestBlockSize);
    reference.loadNamModel(NamSlot::First, SlimmableModel);
    reference.process(input, TestBlockSize, defaultControls());

    const auto actual = loadThroughPlugin(host, input, {{FileSlot::Nam1, invalidPath}});
    const auto expected = reference.process(input, TestBlockSize, defaultControls());

    EXPECT_TRUE(sameAudio(actual, expected)) << invalidPath;
    EXPECT_TRUE(host.hasLogContaining(invalidPath)) << invalidPath;
    EXPECT_EQ(notifiedPath(host.takeNotifications(), FileSlot::Nam1), SlimmableModel)
        << invalidPath;
  }
}

TEST(Lv2Files, PatchGetReportsEveryFileParameter)
{
  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  loadThroughPlugin(host, input,
                    {{FileSlot::Nam1, SlimmableModel}, {FileSlot::CabinetIr, CabinetIr}});
  host.takeNotifications();

  host.requestFiles();
  host.process(firstBlock(input), TestBlockSize);

  const auto notifications = host.takeNotifications();
  EXPECT_EQ(notifiedPath(notifications, FileSlot::Nam1), SlimmableModel);
  EXPECT_EQ(notifiedPath(notifications, FileSlot::Nam2), "");
  EXPECT_EQ(notifiedPath(notifications, FileSlot::CabinetIr), CabinetIr);
}

TEST(Lv2Files, NotificationsThatDoNotFitArriveIntactInLaterRuns)
{
  constexpr size_t PatchSetOverheadAllowance = 128;
  const size_t longestPath =
      std::max({SlimmableModel.size(), SecondModel.size(), CabinetIr.size()});
  HostConfig config;
  config.notifyBufferBytes = sizeof(LV2_Atom_Sequence) + longestPath + PatchSetOverheadAllowance;
  FakeLv2Host host(config);
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);

  loadThroughPlugin(host, input,
                    {{FileSlot::Nam1, SlimmableModel},
                     {FileSlot::Nam2, SecondModel},
                     {FileSlot::CabinetIr, CabinetIr}});

  const auto notifications = host.takeNotifications();
  EXPECT_EQ(notifiedPath(notifications, FileSlot::Nam1), SlimmableModel);
  EXPECT_EQ(notifiedPath(notifications, FileSlot::Nam2), SecondModel);
  EXPECT_EQ(notifiedPath(notifications, FileSlot::CabinetIr), CabinetIr);
  EXPECT_EQ(host.malformedNotificationCount(), 0u);
}

TEST(Lv2Nam, SlimmableCapturesAlwaysRunTheirSlimSubmodel)
{
  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  const auto actual = loadThroughPlugin(host, input, {{FileSlot::Nam1, SlimmableModel}});

  const auto referenceAtQuality = [&input](float percent)
  {
    ReferenceChain reference(TestSampleRate, TestBlockSize);
    reference.setNamQualityPercent(NamSlot::First, percent);
    auto output = reference.process(firstBlock(input), TestBlockSize, defaultControls());
    reference.loadNamModel(NamSlot::First, SlimmableModel);
    return concat(std::move(output),
                  reference.process(afterFirstBlock(input), TestBlockSize, defaultControls()));
  };

  EXPECT_TRUE(sameAudio(actual, referenceAtQuality(0.0f)));
  EXPECT_FALSE(sameAudio(actual, referenceAtQuality(100.0f), 1e-6f));
}

TEST(Lv2RealtimeSafety, RunNeverAllocates)
{
  if (!allocationTrackingAvailable())
    GTEST_SKIP() << "Allocation tracking requires an AddressSanitizer build";

  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  host.process(input, TestBlockSize);

  resetTrackedAllocationCount();
  host.setTrackRunAllocations(true);

  host.process(input, TestBlockSize);
  loadThroughPlugin(host, input,
                    {{FileSlot::Nam1, SlimmableModel},
                     {FileSlot::Nam2, SecondModel},
                     {FileSlot::CabinetIr, CabinetIr}});
  host.requestFiles();
  host.setControl(port::Enabled, 0.0f);
  host.process(input, TestBlockSize);
  host.setControl(port::Enabled, 1.0f);
  loadThroughPlugin(host, input, {{FileSlot::Nam2, ""}, {FileSlot::CabinetIr, ""}});

  host.setTrackRunAllocations(false);
  EXPECT_EQ(trackedAllocationCount(), 0u);
}

TEST(Lv2Bypass, DisabledPluginFadesToDryInputWithoutJumps)
{
  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  loadThroughPlugin(host, input, {{FileSlot::Nam1, SlimmableModel}});
  const auto processedTail = host.process(firstBlock(input), TestBlockSize);

  host.setControl(port::Enabled, 0.0f);
  const auto actual = host.process(input, TestBlockSize);

  const auto rampFrames = static_cast<size_t>(BypassRampSeconds * TestSampleRate);
  const size_t settled = rampFrames + TestBlockSize;
  EXPECT_TRUE(sameAudio({actual.begin() + static_cast<std::ptrdiff_t>(settled), actual.end()},
                        {input.begin() + static_cast<std::ptrdiff_t>(settled), input.end()}));

  float largestInputStep = 0.0f;
  for (size_t i = 1; i < input.size(); ++i)
    largestInputStep = std::max(largestInputStep, std::fabs(input[i] - input[i - 1]));

  float largestStep = std::fabs(actual.front() - processedTail.back());
  for (size_t i = 1; i < settled; ++i)
    largestStep = std::max(largestStep, std::fabs(actual[i] - actual[i - 1]));
  EXPECT_LT(largestStep, largestInputStep * 4.0f + 0.05f);
}

TEST(Lv2Bypass, ReenabledPluginReturnsToProcessedOutput)
{
  FakeLv2Host host;
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  ReferenceChain reference(TestSampleRate, TestBlockSize);

  host.setControl(port::Enabled, 0.0f);
  host.process(input, TestBlockSize);
  reference.process(input, TestBlockSize, defaultControls());

  host.setControl(port::Enabled, 1.0f);
  const auto actual = host.process(input, TestBlockSize);
  const auto expected = reference.process(input, TestBlockSize, defaultControls());

  const auto settled =
      static_cast<std::ptrdiff_t>(BypassRampSeconds * TestSampleRate) + TestBlockSize;
  EXPECT_TRUE(sameAudio({actual.begin() + settled, actual.end()},
                        {expected.begin() + settled, expected.end()}));
}

TEST(Lv2Blocks, BlocksLongerThanTheProcessingChunkAreSplit)
{
  HostConfig config;
  config.provideOptions = false;
  FakeLv2Host host(config);
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, 1.0);

  loadThroughPlugin(host, input,
                    {{FileSlot::Nam1, SlimmableModel}, {FileSlot::CabinetIr, CabinetIr}});
  const auto longBlocks = host.process(input, 8192);

  ReferenceChain reference(TestSampleRate, 64);
  reference.loadNamModel(NamSlot::First, SlimmableModel);
  reference.loadImpulseResponse(CabinetIr);
  reference.process(input, 64, defaultControls());
  const auto expected = reference.process(input, 64, defaultControls());

  EXPECT_TRUE(sameAudio(longBlocks, expected, 1e-4f));
}

TEST(Lv2State, SaveAndRestoreRoundTripsLoadedFiles)
{
  HostConfig config;
  config.stateDirectory = TEST_DATA_DIR;
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);

  StoredState saved;
  {
    FakeLv2Host original(config);
    ASSERT_TRUE(original.isInstantiated());
    loadThroughPlugin(original, input,
                      {{FileSlot::Nam1, SlimmableModel},
                       {FileSlot::Nam2, SecondModel},
                       {FileSlot::CabinetIr, CabinetIr}});
    saved = original.saveState();
  }

  ASSERT_EQ(saved.count(fileParameter(FileSlot::Nam1).uri), 1u);
  const auto& savedNam1 = saved.at(fileParameter(FileSlot::Nam1).uri);
  EXPECT_EQ(std::string(savedNam1.bytes.begin(), savedNam1.bytes.end()).c_str(),
            std::string("INPUT_HM2-W OctoBASS distortion 2_a2.nam"));

  FakeLv2Host restored(config);
  ASSERT_TRUE(restored.isInstantiated());
  restored.restoreState(saved);
  const auto actual = restored.process(input, TestBlockSize);

  ReferenceChain reference(TestSampleRate, TestBlockSize);
  reference.loadNamModel(NamSlot::First, SlimmableModel);
  reference.loadNamModel(NamSlot::Second, SecondModel);
  reference.loadImpulseResponse(CabinetIr);
  EXPECT_TRUE(sameAudio(actual, reference.process(input, TestBlockSize, defaultControls())));

  restored.requestFiles();
  restored.process(firstBlock(input), TestBlockSize);
  const auto notifications = restored.takeNotifications();
  EXPECT_EQ(notifiedPath(notifications, FileSlot::Nam1), SlimmableModel);
  EXPECT_EQ(notifiedPath(notifications, FileSlot::Nam2), SecondModel);
  EXPECT_EQ(notifiedPath(notifications, FileSlot::CabinetIr), CabinetIr);
}

TEST(Lv2State, SlotClearedBeforeSavingStaysEmptyAfterRestore)
{
  HostConfig config;
  config.stateDirectory = TEST_DATA_DIR;
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);

  FakeLv2Host host(config);
  ASSERT_TRUE(host.isInstantiated());
  loadThroughPlugin(host, input,
                    {{FileSlot::Nam1, SlimmableModel},
                     {FileSlot::Nam2, SecondModel},
                     {FileSlot::CabinetIr, CabinetIr}});
  loadThroughPlugin(host, input, {{FileSlot::Nam2, ""}});
  const auto saved = host.saveState();
  EXPECT_EQ(saved.count(fileParameter(FileSlot::Nam2).uri), 0u);

  FakeLv2Host restored(config);
  ASSERT_TRUE(restored.isInstantiated());
  loadThroughPlugin(restored, input, {{FileSlot::Nam2, SecondModel}});
  restored.restoreState(saved);
  const auto actual = restored.process(input, TestBlockSize);

  ReferenceChain reference(TestSampleRate, TestBlockSize);
  reference.loadNamModel(NamSlot::First, SlimmableModel);
  reference.loadImpulseResponse(CabinetIr);
  const auto expected = reference.process(input, TestBlockSize, defaultControls());
  const std::vector<float> settledActual(actual.end() - TestBlockSize, actual.end());
  const std::vector<float> settledExpected(expected.end() - TestBlockSize, expected.end());
  EXPECT_TRUE(sameAudio(settledActual, settledExpected, 1e-3f));

  restored.takeNotifications();
  restored.requestFiles();
  restored.process(firstBlock(input), TestBlockSize);
  EXPECT_EQ(notifiedPath(restored.takeNotifications(), FileSlot::Nam2), "");
}

TEST(Lv2State, RestoringMissingFileClearsSlotAndLogs)
{
  HostConfig config;
  config.stateDirectory = TEST_DATA_DIR;
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);

  FakeLv2Host host(config);
  ASSERT_TRUE(host.isInstantiated());
  loadThroughPlugin(host, input, {{FileSlot::Nam1, SlimmableModel}});
  auto state = host.saveState();

  const std::string missing = "missing_model.nam";
  state[fileParameter(FileSlot::Nam1).uri].bytes.assign(missing.begin(), missing.end());
  state[fileParameter(FileSlot::Nam1).uri].bytes.push_back(0);
  host.restoreState(state);

  const auto actual = host.process(input, TestBlockSize);
  ReferenceChain reference(TestSampleRate, TestBlockSize);
  const auto expected = reference.process(input, TestBlockSize, defaultControls());
  const std::vector<float> settledActual(actual.end() - TestBlockSize, actual.end());
  const std::vector<float> settledExpected(expected.end() - TestBlockSize, expected.end());
  EXPECT_TRUE(sameAudio(settledActual, settledExpected, 1e-3f));
  EXPECT_TRUE(host.hasLogContaining(missing));
}

TEST(Lv2SampleRates, ProcessesFiniteAudioAtCommonRates)
{
  for (const double rate : {44100.0, 48000.0, 96000.0})
  {
    HostConfig config;
    config.sampleRate = rate;
    FakeLv2Host host(config);
    ASSERT_TRUE(host.isInstantiated()) << rate;
    const auto input = generateBassSignal(rate, SignalSeconds);
    const auto output = loadThroughPlugin(
        host, input, {{FileSlot::Nam1, SlimmableModel}, {FileSlot::CabinetIr, CabinetIr}});
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](float s) { return std::isfinite(s); }))
        << rate;
    EXPECT_GT(*std::max_element(output.begin(), output.end()), 0.0f) << rate;
  }
}

TEST(Lv2Robustness, InstantiationFailureIsLoggedAndReportedToHost)
{
  HostConfig config;
  config.uriThatFailsToMap = fileParameter(FileSlot::CabinetIr).uri;

  std::unique_ptr<FakeLv2Host> host;
  ASSERT_NO_THROW(host = std::make_unique<FakeLv2Host>(config));
  EXPECT_FALSE(host->isInstantiated());
  EXPECT_TRUE(host->hasLogContaining("host failed to map"));
}

TEST(Lv2Robustness, StateFailuresAreLoggedReportedToHostAndLeaveThePluginUsable)
{
  HostConfig config;
  config.stateCallbacksThrow = true;
  FakeLv2Host host(config);
  ASSERT_TRUE(host.isInstantiated());
  const auto input = generateBassSignal(TestSampleRate, SignalSeconds);
  loadThroughPlugin(host, input, {{FileSlot::Nam1, SlimmableModel}});

  ASSERT_NO_THROW(host.saveState());
  EXPECT_EQ(host.lastStateStatus(), LV2_STATE_ERR_UNKNOWN);
  EXPECT_TRUE(host.hasLogContaining("host failed to store state"));

  ASSERT_NO_THROW(host.restoreState({}));
  EXPECT_EQ(host.lastStateStatus(), LV2_STATE_ERR_UNKNOWN);
  EXPECT_TRUE(host.hasLogContaining("host failed to retrieve state"));

  host.takeNotifications();
  loadThroughPlugin(host, input, {{FileSlot::Nam2, SecondModel}});
  EXPECT_EQ(notifiedPath(host.takeNotifications(), FileSlot::Nam2), SecondModel);
}
