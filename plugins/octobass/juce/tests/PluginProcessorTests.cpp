#include <gtest/gtest.h>

#include <cmath>
#include <octobass-core/GraphicEQ.hpp>
#include <octobass-core/Types.hpp>
#include <string>
#include <utility>
#include <vector>

#include "GraphicEQDisplay.h"
#include "PluginProcessor.h"

static const std::string kIrPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_a.wav";

class OctoBassProcessorTest : public ::testing::Test
{
 protected:
  OctoBassProcessor processor;
};

TEST_F(OctoBassProcessorTest, CreateSuccessfully)
{
  EXPECT_EQ(processor.getName(), "OctoBASS");
}

TEST_F(OctoBassProcessorTest, HasEditor)
{
  EXPECT_TRUE(processor.hasEditor());
}

TEST_F(OctoBassProcessorTest, DoesNotAcceptMidi)
{
  EXPECT_FALSE(processor.acceptsMidi());
  EXPECT_FALSE(processor.producesMidi());
  EXPECT_FALSE(processor.isMidiEffect());
}

TEST_F(OctoBassProcessorTest, MagnitudePreserved)
{
  const double sampleRate = 44100.0;
  const int blockSize = 512;
  const int numBlocks = 16;
  const int totalSamples = numBlocks * blockSize;
  const int skipSamples = 4 * blockSize;

  processor.prepareToPlay(sampleRate, blockSize);

  // Generate a test tone
  juce::AudioBuffer<float> inputBuf(1, totalSamples);
  for (int i = 0; i < totalSamples; ++i)
    inputBuf.setSample(0, i,
                       0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 100.0f *
                                       static_cast<float>(i) / static_cast<float>(sampleRate)));

  // Process in blocks
  juce::AudioBuffer<float> buffer(1, blockSize);
  juce::MidiBuffer midi;
  std::vector<float> output(static_cast<size_t>(totalSamples));

  for (int b = 0; b < numBlocks; ++b)
  {
    for (int i = 0; i < blockSize; ++i)
      buffer.setSample(0, i, inputBuf.getSample(0, b * blockSize + i));

    processor.processBlock(buffer, midi);

    for (int i = 0; i < blockSize; ++i)
      output[static_cast<size_t>(b * blockSize + i)] = buffer.getSample(0, i);
  }

  // Compare RMS after warmup (crossover filter has transient)
  double inputRMS = 0.0, outputRMS = 0.0;
  for (int i = skipSamples; i < totalSamples; ++i)
  {
    float in = inputBuf.getSample(0, i);
    inputRMS += static_cast<double>(in) * in;
    outputRMS +=
        static_cast<double>(output[static_cast<size_t>(i)]) * output[static_cast<size_t>(i)];
  }
  int compareLen = totalSamples - skipSamples;
  inputRMS = std::sqrt(inputRMS / compareLen);
  outputRMS = std::sqrt(outputRMS / compareLen);

  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);
  EXPECT_NEAR(ratioDb, 0.0, 1.0) << "Default settings should preserve magnitude";

  processor.releaseResources();
}

TEST_F(OctoBassProcessorTest, SquashParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("squash");
  ASSERT_NE(param, nullptr);
  EXPECT_FLOAT_EQ(param->load(), 0.0f);
}

TEST_F(OctoBassProcessorTest, CompressionModeParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("compressionMode");
  ASSERT_NE(param, nullptr);
  EXPECT_FLOAT_EQ(param->load(), 0.0f);
}

TEST_F(OctoBassProcessorTest, CrossoverParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("crossoverFrequency");
  ASSERT_NE(param, nullptr);
  EXPECT_FLOAT_EQ(param->load(), 250.0f);
}

TEST_F(OctoBassProcessorTest, LowBandLevelParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("lowBandLevel");
  ASSERT_NE(param, nullptr);
  EXPECT_NEAR(param->load(), 0.0f, 0.01f);
}

TEST_F(OctoBassProcessorTest, HighInputGainParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("highInputGain");
  ASSERT_NE(param, nullptr);
  EXPECT_NEAR(param->load(), 0.0f, 0.01f);
}

TEST_F(OctoBassProcessorTest, HighOutputGainParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("highOutputGain");
  ASSERT_NE(param, nullptr);
  EXPECT_NEAR(param->load(), 0.0f, 0.01f);
}

TEST_F(OctoBassProcessorTest, NamNotLoadedByDefault)
{
  EXPECT_FALSE(processor.isNamModelLoaded());
}

TEST_F(OctoBassProcessorTest, NamQualityParameterExists)
{
  auto* param = processor.getAPVTS().getRawParameterValue("namQuality");
  ASSERT_NE(param, nullptr);
  EXPECT_NEAR(param->load(), octob::DefaultNamQuality, 0.01f)
      << "NAM quality should default to full quality";
}

TEST_F(OctoBassProcessorTest, StateRoundTripWithNamQuality)
{
  auto* param = processor.getAPVTS().getParameter("namQuality");
  ASSERT_NE(param, nullptr);
  param->setValueNotifyingHost(param->convertTo0to1(0.25f));

  juce::MemoryBlock stateData;
  processor.getStateInformation(stateData);

  OctoBassProcessor processor2;
  processor2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

  EXPECT_NEAR(processor2.getAPVTS().getRawParameterValue("namQuality")->load(), 0.25f, 0.02f);
}

TEST_F(OctoBassProcessorTest, IRNotLoadedByDefault)
{
  EXPECT_FALSE(processor.isIRLoaded());
  EXPECT_TRUE(processor.getCurrentIRPath().isEmpty());
}

// Helper: process one silent block to flush pending IR updates through the
// IRProcessor staging mechanism (IR swap happens during processMono).
static void processOneBlock(OctoBassProcessor& proc, int blockSize = 512)
{
  juce::AudioBuffer<float> buf(1, blockSize);
  buf.clear();
  juce::MidiBuffer midi;
  proc.processBlock(buf, midi);
}

TEST_F(OctoBassProcessorTest, LoadImpulseResponse)
{
  processor.prepareToPlay(44100.0, 512);

  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse(kIrPath, err)) << err;
  EXPECT_EQ(processor.getCurrentIRPath(), juce::String(kIrPath));

  processOneBlock(processor);
  EXPECT_TRUE(processor.isIRLoaded());
}

TEST_F(OctoBassProcessorTest, ClearImpulseResponse)
{
  processor.prepareToPlay(44100.0, 512);

  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse(kIrPath, err)) << err;
  processOneBlock(processor);
  EXPECT_TRUE(processor.isIRLoaded());

  processor.clearImpulseResponse();
  processOneBlock(processor);
  EXPECT_FALSE(processor.isIRLoaded());
  EXPECT_TRUE(processor.getCurrentIRPath().isEmpty());
}

TEST_F(OctoBassProcessorTest, LatencyConsistentWithCore)
{
  processor.prepareToPlay(44100.0, 512);
  EXPECT_EQ(processor.getLatencySamples(), 0);

  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse(kIrPath, err)) << err;
  processOneBlock(processor);

  // Latency should be non-negative and consistent across calls
  int latency = processor.getLatencySamples();
  EXPECT_GE(latency, 0);
  EXPECT_EQ(latency, processor.getLatencySamples());
}

TEST_F(OctoBassProcessorTest, StateRoundTrip)
{
  // Modify some parameters
  auto* squashParam = processor.getAPVTS().getParameter("squash");
  auto* modeParam = processor.getAPVTS().getParameter("compressionMode");
  ASSERT_NE(squashParam, nullptr);
  ASSERT_NE(modeParam, nullptr);

  squashParam->setValueNotifyingHost(0.7f);
  modeParam->setValueNotifyingHost(modeParam->convertTo0to1(2.0f));  // Punch mode

  // Save state
  juce::MemoryBlock stateData;
  processor.getStateInformation(stateData);
  EXPECT_GT(stateData.getSize(), 0u);

  // Restore into a new processor
  OctoBassProcessor processor2;
  processor2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

  auto* squash2 = processor2.getAPVTS().getRawParameterValue("squash");
  auto* mode2 = processor2.getAPVTS().getRawParameterValue("compressionMode");
  ASSERT_NE(squash2, nullptr);
  ASSERT_NE(mode2, nullptr);

  EXPECT_NEAR(squash2->load(), 0.7f, 0.02f);
  EXPECT_NEAR(mode2->load(), 2.0f, 0.1f);
}

TEST_F(OctoBassProcessorTest, EQNodeParametersExist)
{
  for (int i = 0; i < octob::kGraphicEQNumNodes; ++i)
  {
    auto slot = juce::String(i);
    auto* active = processor.getAPVTS().getRawParameterValue("eqNodeActive" + slot);
    auto* freq = processor.getAPVTS().getRawParameterValue("eqNodeFreq" + slot);
    auto* gain = processor.getAPVTS().getRawParameterValue("eqNodeGain" + slot);

    ASSERT_NE(active, nullptr) << "Missing active parameter for node " << i;
    ASSERT_NE(freq, nullptr) << "Missing freq parameter for node " << i;
    ASSERT_NE(gain, nullptr) << "Missing gain parameter for node " << i;

    EXPECT_LT(active->load(), 0.5f) << "Node " << i << " should default to inactive";
    EXPECT_NEAR(freq->load(), octob::DefaultGraphicEQFreqHz, 1.0f)
        << "Node " << i << " should default to 1 kHz";
    EXPECT_NEAR(gain->load(), 0.0f, 0.01f) << "Node " << i << " should default to 0 dB";
  }
}

TEST_F(OctoBassProcessorTest, StateRoundTripWithNodes)
{
  auto setNodeParams = [this](int slot, float freqHz, float gainDb)
  {
    auto slotStr = juce::String(slot);
    auto* active = processor.getAPVTS().getParameter("eqNodeActive" + slotStr);
    auto* freq = processor.getAPVTS().getParameter("eqNodeFreq" + slotStr);
    auto* gain = processor.getAPVTS().getParameter("eqNodeGain" + slotStr);
    ASSERT_NE(active, nullptr);
    ASSERT_NE(freq, nullptr);
    ASSERT_NE(gain, nullptr);
    active->setValueNotifyingHost(1.0f);
    freq->setValueNotifyingHost(freq->convertTo0to1(freqHz));
    gain->setValueNotifyingHost(gain->convertTo0to1(gainDb));
  };

  setNodeParams(2, 250.0f, 6.0f);
  setNodeParams(5, 3000.0f, -3.0f);

  juce::MemoryBlock stateData;
  processor.getStateInformation(stateData);

  OctoBassProcessor processor2;
  processor2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

  auto& apvts2 = processor2.getAPVTS();
  EXPECT_GE(apvts2.getRawParameterValue("eqNodeActive2")->load(), 0.5f);
  EXPECT_NEAR(apvts2.getRawParameterValue("eqNodeFreq2")->load(), 250.0f, 2.0f);
  EXPECT_NEAR(apvts2.getRawParameterValue("eqNodeGain2")->load(), 6.0f, 0.2f);

  EXPECT_GE(apvts2.getRawParameterValue("eqNodeActive5")->load(), 0.5f);
  EXPECT_NEAR(apvts2.getRawParameterValue("eqNodeFreq5")->load(), 3000.0f, 20.0f);
  EXPECT_NEAR(apvts2.getRawParameterValue("eqNodeGain5")->load(), -3.0f, 0.2f);

  EXPECT_LT(apvts2.getRawParameterValue("eqNodeActive0")->load(), 0.5f)
      << "Untouched slots must stay inactive after a round trip";
}

namespace
{

// Build a binary state block in the legacy fixed-band format (eqBandGain0..23)
juce::MemoryBlock buildLegacyState(const std::vector<std::pair<int, float>>& bandGains)
{
  juce::ValueTree state("OctoBassParams");
  for (const auto& [band, gainDb] : bandGains)
  {
    juce::ValueTree param("PARAM");
    param.setProperty("id", "eqBandGain" + juce::String(band), nullptr);
    param.setProperty("value", gainDb, nullptr);
    state.appendChild(param, nullptr);
  }

  juce::MemoryBlock data;
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  juce::AudioProcessor::copyXmlToBinary(*xml, data);
  return data;
}

}  // namespace

TEST_F(OctoBassProcessorTest, OldStateMigration)
{
  auto stateData = buildLegacyState({{5, 6.0f}, {14, -3.0f}});
  processor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

  auto& apvts = processor.getAPVTS();

  // Slots assigned in ascending band order: slot 0 = band 5, slot 1 = band 14
  EXPECT_GE(apvts.getRawParameterValue("eqNodeActive0")->load(), 0.5f);
  EXPECT_NEAR(apvts.getRawParameterValue("eqNodeFreq0")->load(),
              octob::GraphicEQ::kLegacyCenterFreqs[5], 1.0f);
  EXPECT_NEAR(apvts.getRawParameterValue("eqNodeGain0")->load(), 6.0f, 0.2f);

  EXPECT_GE(apvts.getRawParameterValue("eqNodeActive1")->load(), 0.5f);
  EXPECT_NEAR(apvts.getRawParameterValue("eqNodeFreq1")->load(),
              octob::GraphicEQ::kLegacyCenterFreqs[14], 5.0f);
  EXPECT_NEAR(apvts.getRawParameterValue("eqNodeGain1")->load(), -3.0f, 0.2f);

  for (int i = 2; i < octob::kGraphicEQNumNodes; ++i)
    EXPECT_LT(apvts.getRawParameterValue("eqNodeActive" + juce::String(i))->load(), 0.5f)
        << "Slot " << i << " should stay inactive after migrating two bands";
}

TEST_F(OctoBassProcessorTest, OldStateMigration_MoreThanNodeLimit)
{
  // 20 non-zero bands: only the 16 largest magnitudes (bands 0..15) must win
  std::vector<std::pair<int, float>> bandGains;
  for (int band = 0; band < 20; ++band)
  {
    float gainDb = (12.0f - 0.5f * static_cast<float>(band)) * ((band % 2 == 0) ? 1.0f : -1.0f);
    bandGains.emplace_back(band, gainDb);
  }

  auto stateData = buildLegacyState(bandGains);
  processor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

  auto& apvts = processor.getAPVTS();
  for (int slot = 0; slot < octob::kGraphicEQNumNodes; ++slot)
  {
    auto slotStr = juce::String(slot);
    EXPECT_GE(apvts.getRawParameterValue("eqNodeActive" + slotStr)->load(), 0.5f)
        << "Slot " << slot << " should be active";

    // Slots follow ascending band order, so slot index == legacy band index here
    float expectedFreq =
        juce::jlimit(octob::MinGraphicEQFreqHz, octob::MaxGraphicEQFreqHz,
                     octob::GraphicEQ::kLegacyCenterFreqs[static_cast<size_t>(slot)]);
    float expectedGain =
        (12.0f - 0.5f * static_cast<float>(slot)) * ((slot % 2 == 0) ? 1.0f : -1.0f);

    EXPECT_NEAR(apvts.getRawParameterValue("eqNodeFreq" + slotStr)->load(), expectedFreq,
                expectedFreq * 0.01f + 0.5f);
    EXPECT_NEAR(apvts.getRawParameterValue("eqNodeGain" + slotStr)->load(), expectedGain, 0.2f);
  }
}

TEST_F(OctoBassProcessorTest, CutParametersExist)
{
  auto& apvts = processor.getAPVTS();

  auto* lowActive = apvts.getRawParameterValue("eqLowCutActive");
  auto* lowFreq = apvts.getRawParameterValue("eqLowCutFreq");
  auto* highActive = apvts.getRawParameterValue("eqHighCutActive");
  auto* highFreq = apvts.getRawParameterValue("eqHighCutFreq");

  ASSERT_NE(lowActive, nullptr);
  ASSERT_NE(lowFreq, nullptr);
  ASSERT_NE(highActive, nullptr);
  ASSERT_NE(highFreq, nullptr);

  EXPECT_GE(lowActive->load(), 0.5f)
      << "Low cut should default to active so users can see the handle";
  EXPECT_NEAR(lowFreq->load(), octob::MinGraphicEQFreqHz, 0.5f)
      << "Low cut should default to the bottom of the range where it is transparent";
  EXPECT_GE(highActive->load(), 0.5f)
      << "High cut should default to active so users can see the handle";
  EXPECT_NEAR(highFreq->load(), octob::MaxGraphicEQFreqHz, 50.0f)
      << "High cut should default to the top of the range where it is transparent";
}

TEST_F(OctoBassProcessorTest, StateRoundTripWithCuts)
{
  auto& apvts = processor.getAPVTS();
  apvts.getParameter("eqLowCutActive")->setValueNotifyingHost(1.0f);
  auto* lowFreq = apvts.getParameter("eqLowCutFreq");
  lowFreq->setValueNotifyingHost(lowFreq->convertTo0to1(80.0f));
  apvts.getParameter("eqHighCutActive")->setValueNotifyingHost(1.0f);
  auto* highFreq = apvts.getParameter("eqHighCutFreq");
  highFreq->setValueNotifyingHost(highFreq->convertTo0to1(5000.0f));

  juce::MemoryBlock stateData;
  processor.getStateInformation(stateData);

  OctoBassProcessor processor2;
  processor2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

  auto& apvts2 = processor2.getAPVTS();
  EXPECT_GE(apvts2.getRawParameterValue("eqLowCutActive")->load(), 0.5f);
  EXPECT_NEAR(apvts2.getRawParameterValue("eqLowCutFreq")->load(), 80.0f, 1.0f);
  EXPECT_GE(apvts2.getRawParameterValue("eqHighCutActive")->load(), 0.5f);
  EXPECT_NEAR(apvts2.getRawParameterValue("eqHighCutFreq")->load(), 5000.0f, 30.0f);
}

TEST_F(OctoBassProcessorTest, NewStateNotMigrated)
{
  // A state that already contains node parameters must not trigger migration
  auto* active7 = processor.getAPVTS().getParameter("eqNodeActive7");
  auto* gain7 = processor.getAPVTS().getParameter("eqNodeGain7");
  ASSERT_NE(active7, nullptr);
  ASSERT_NE(gain7, nullptr);
  active7->setValueNotifyingHost(1.0f);
  gain7->setValueNotifyingHost(gain7->convertTo0to1(-9.0f));

  juce::MemoryBlock stateData;
  processor.getStateInformation(stateData);

  OctoBassProcessor processor2;
  processor2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

  auto& apvts2 = processor2.getAPVTS();
  EXPECT_GE(apvts2.getRawParameterValue("eqNodeActive7")->load(), 0.5f);
  EXPECT_NEAR(apvts2.getRawParameterValue("eqNodeGain7")->load(), -9.0f, 0.2f);
  EXPECT_LT(apvts2.getRawParameterValue("eqNodeActive0")->load(), 0.5f);
}

TEST(GraphicEQDisplayMapping, FreqNormXRoundTrip)
{
  EXPECT_NEAR(GraphicEQDisplay::freqToNormX(GraphicEQDisplay::kMinFreqHz), 0.0f, 1e-5f);
  EXPECT_NEAR(GraphicEQDisplay::freqToNormX(GraphicEQDisplay::kMaxFreqHz), 1.0f, 1e-5f);

  for (float freq : {20.0f, 100.0f, 440.0f, 1000.0f, 5000.0f, 12600.0f, 20000.0f})
  {
    float roundTrip = GraphicEQDisplay::normXToFreq(GraphicEQDisplay::freqToNormX(freq));
    EXPECT_NEAR(roundTrip, freq, freq * 0.001f) << "Round trip failed for " << freq << " Hz";
  }

  EXPECT_NEAR(GraphicEQDisplay::normXToFreq(-0.5f), GraphicEQDisplay::kMinFreqHz, 0.01f)
      << "Out-of-range normX should clamp to the minimum frequency";
  EXPECT_NEAR(GraphicEQDisplay::normXToFreq(1.5f), GraphicEQDisplay::kMaxFreqHz, 1.0f)
      << "Out-of-range normX should clamp to the maximum frequency";
}

TEST(GraphicEQDisplayMapping, HitTestPicksNearestWithinRadius)
{
  constexpr int kCount = 3;
  bool active[kCount] = {true, true, true};
  float xs[kCount] = {100.0f, 110.0f, 300.0f};
  float ys[kCount] = {50.0f, 50.0f, 50.0f};

  EXPECT_EQ(GraphicEQDisplay::nearestNodeIndex(active, xs, ys, kCount, 101.0f, 50.0f, 8.0f), 0);
  EXPECT_EQ(GraphicEQDisplay::nearestNodeIndex(active, xs, ys, kCount, 108.0f, 50.0f, 8.0f), 1);
  EXPECT_EQ(GraphicEQDisplay::nearestNodeIndex(active, xs, ys, kCount, 300.0f, 55.0f, 8.0f), 2);
}

TEST(GraphicEQDisplayMapping, HitTestMissesOutsideRadius)
{
  constexpr int kCount = 2;
  bool active[kCount] = {true, false};
  float xs[kCount] = {100.0f, 200.0f};
  float ys[kCount] = {50.0f, 50.0f};

  EXPECT_EQ(GraphicEQDisplay::nearestNodeIndex(active, xs, ys, kCount, 150.0f, 50.0f, 8.0f), -1)
      << "Point far from all nodes must miss";
  EXPECT_EQ(GraphicEQDisplay::nearestNodeIndex(active, xs, ys, kCount, 200.0f, 50.0f, 8.0f), -1)
      << "Inactive nodes must not be hit";
}

TEST(GraphicEQDisplayMapping, CutZones)
{
  // The cut zones span one spectrum-bar width (1/24) at each edge
  EXPECT_TRUE(GraphicEQDisplay::isInLowCutZone(0.0f));
  EXPECT_TRUE(GraphicEQDisplay::isInLowCutZone(GraphicEQDisplay::kCutZoneNormWidth - 0.001f));
  EXPECT_FALSE(GraphicEQDisplay::isInLowCutZone(GraphicEQDisplay::kCutZoneNormWidth + 0.001f));
  EXPECT_FALSE(GraphicEQDisplay::isInLowCutZone(0.5f));

  EXPECT_TRUE(GraphicEQDisplay::isInHighCutZone(1.0f));
  EXPECT_TRUE(
      GraphicEQDisplay::isInHighCutZone(1.0f - GraphicEQDisplay::kCutZoneNormWidth + 0.001f));
  EXPECT_FALSE(
      GraphicEQDisplay::isInHighCutZone(1.0f - GraphicEQDisplay::kCutZoneNormWidth - 0.001f));
  EXPECT_FALSE(GraphicEQDisplay::isInHighCutZone(0.5f));

  EXPECT_FALSE(GraphicEQDisplay::isInLowCutZone(0.5f) || GraphicEQDisplay::isInHighCutZone(0.5f))
      << "The center of the display must create peak nodes, not cuts";
}

TEST_F(OctoBassProcessorTest, StateRoundTripWithIRPath)
{
  processor.prepareToPlay(44100.0, 512);

  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse(kIrPath, err)) << err;

  juce::MemoryBlock stateData;
  processor.getStateInformation(stateData);
  EXPECT_GT(stateData.getSize(), 0u);

  // Verify the IR path is serialized in the state XML
  std::unique_ptr<juce::XmlElement> xml(
      processor.getXmlFromBinary(stateData.getData(), static_cast<int>(stateData.getSize())));
  ASSERT_NE(xml, nullptr);
  EXPECT_TRUE(xml->hasAttribute("irPath"));
  EXPECT_EQ(xml->getStringAttribute("irPath"), juce::String(kIrPath));
}
